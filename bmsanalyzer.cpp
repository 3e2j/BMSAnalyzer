#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <tuple>
#include <iomanip> 
#include <algorithm>
#include <stack>
#include <unordered_set>

/* BMS to MIDI converter

- AZ

 */

struct TrackEvent {
    std::string event;
    std::string description;
};

// Thanks XAYRGA for most track keys
enum MML {
    OPEN_TRACK          = 0xC1,
    NOTE_TRACK          = 0xF9,
    WAIT_8              = 0x80,
    WAIT_16             = 0x88,
    WAIT_VAR            = 0xF0,
    CALL                = 0xC3,
    RET                 = 0xC5,
    JUMP                = 0xC7,
    FIN                 = 0xFF,

    J2_SET_PERF_8       = 0xB8,
    J2_SET_PERF_16      = 0xB9,
    J2_SET_ARTIC        = 0xD8,
    J2_TEMPO            = 0xE0,
    J2_SET_BANK         = 0xE2,
    J2_SET_PROG         = 0xE3,
    
    /* Unused / Unimplemented
    Many of these are subject to change from J2's audio system. 
    Some are named their variables as they've been spotted, but unidentified*/
    
    // PARAM_SET           = 0xA0,
    // ADDR                = 0xA1,
    // MULR                = 0xA2,
    // CMPR                = 0xA3,
    // PARAM_SET_8         = 0xA4, 
    // ADD8                = 0xA5,
    // MUL8                = 0xA6,
    // CMP8                = 0xA7,
    // BITWISE             = 0xA9,
    // LOADTBL             = 0xAA,
    // SUB                 = 0xAB,
    // PARAM_SET_16        = 0xAC,
    // ADD16               = 0xAD,
    // MUL16               = 0xAE,
    // CMP16               = 0xAF,
    // LOAD_TABLE          = 0xAA,
    // SUBTRACT            = 0xAB,

    // OSCILLATORFULL      = 0xF2,  
    // FA                  = 0xFA, // 0
    // PRINTF              = 0xFB,
    // NAME_CHECK          = 0xFD,
    // TEMPO               = 0xFE,

    // INTERRUPT_TIMER     = 0xE4,
    // SYNC_CPU            = 0xE7,
    // WAIT_24             = 0xEA,
    // EB                  = 0xEB, // 0
    // PANSWSET            = 0xEF,

    // NAME_BUS            = 0xD0, // 2
    // D1                  = 0xD1, // 2
    // D5                  = 0xD5, // 0
    // ADSR                = 0xD8,
    // D9                  = 0xD9, // 3
    // DA                  = 0xDA, // 4
    // DC                  = 0xDC, // 11
    // BUS_CONNECT         = 0xDD,
    // INTERRUPT           = 0xDF,

    // OPEN_TRACK_BROS     = 0xC2,
    // CALL_COND           = 0xC4, // 4
    // RET_COND            = 0xC6, // 1
    // JUMP_COND           = 0xC8, // 4
    // LOOP_COUNT          = 0xC9,
    // PORTREAD            = 0xCB,
    // PORTWRITE           = 0xCC,
    // SPECIALWAIT         = 0xCF,

    // PERF_U8_NODUR       = 0x94,
    // PERF_U8_DUR_U8      = 0x96,
    // PERF_U8_DUR_U16     = 0x97,
    // PERF_S8_NODUR       = 0x98,
    // PERF_S8_DUR_U8      = 0x9A,
    // PERF_S8_DUR_U16     = 0x9B,
    // PERF_S16_NODUR      = 0x9C,
    // PERF_S16_DUR_U8     = 0x9E,
    // PERF_S16_DUR_U16    = 0x9F,

};

enum EffectType {
    MML_VOLUME = 0,
    MML_PITCH = 1,
    MML_REVERB = 2,
    MML_PAN = 3,
    MML_EFFECT_UNKNOWN = 4
};

std::vector<std::tuple<uint8_t, uint8_t>> trackInstruments;

struct TrackParser {
    std::vector<unsigned char> hexData;
    uint32_t curOffset;
    std::vector<TrackEvent> events;

    TrackParser() : curOffset(0) {}

    int16_t ppqn = 0x0078; // Pulses per Quarter Note (default 120)
    int32_t tempo = 0x491803; // Tempo (default of 4790275 MPQN [microseconds per quarter note])
    std::vector<std::tuple<uint8_t, uint32_t, uint32_t>> trackList; // TrackList [trackNo, trackStart, trackEnd]

    uint8_t voiceToNote[8] = {}; // Array to remember the current note played by each voice ID

    struct StackFrame {
        uint32_t retOffset;
    };

    std::stack<StackFrame> callStack; // Call return positions

    uint32_t VisitedAddressMax = 0;
    std::unordered_set<uint32_t> VisitedAddresses;

    uint32_t trackStartGlob = 0;

    uint8_t trackNum = 0x00;

    bool firstTrack = true;

    /*Track Decoding*/
    void parseEvents(uint32_t trackStart, uint32_t trackEnd) {

        curOffset = trackStart;
        trackStartGlob = trackStart;

        while (curOffset != trackEnd) {
            uint32_t beginOffset = curOffset;
            uint8_t status_byte = hexData[curOffset++];

            //std::cout << std::hex << static_cast<int>(status_byte) << std::endl;

            if (status_byte < 0x80) {
                // Note On Event
                uint8_t note = status_byte;
                uint8_t voice = hexData[curOffset++];
                uint8_t velocity = hexData[curOffset++];

                if (voice <= 0x01 && voice >= 0x08) {
                        if (firstTrack) {
                            firstTrackErrorHandling(status_byte);
                            return;
                        } else {
                            std::cout << "! ERROR: A Note byte could not be read. !" << std::endl;
                            std::cout << "Track Number: " << static_cast<int>(trackNum) << std::endl;
                            std::cout << "Previous Byte: 0x" << std::hex << static_cast<int>(hexData[curOffset-2]) << std::endl;
                            std::cout << "Status Byte: 0x" << std::hex << static_cast<int>(status_byte) << std::endl;
                            std::cout << "Offset: 0x" << std::hex << static_cast<int>(curOffset) << std::endl;
                            throw;
                        }
                };

                onEvent();
                handleNoteOn(note, velocity);
            } else if (status_byte == WAIT_8) {
                uint8_t waitTime = hexData[curOffset++];
                addTime(waitTime);    
            } else if (status_byte < 0x88) {
                // Note Off Event
                uint8_t voice = status_byte & ~0x80;

                onEvent();
                handleNoteOff(voice);
            } else {
                // Other event handling
                switch (status_byte) {
                    case WAIT_16: {
                        uint16_t waitTime = read16();
                        addTime(waitTime);
                        onEvent();
                        break;
                    }
                    case WAIT_VAR: {
                        uint32_t waitTime = convertFromVLQ();
                        addTime(waitTime);
                        onEvent();
                        break;
                    }
                    case JUMP: {
                        uint32_t jumpOffset = read24();

                        // Check if the jump offset is beyond the current position
                        if (isOffsetUsed(jumpOffset)) {
                            onEvent();
                            curOffset = jumpOffset;
                        } else {
                            // Jump offset points backward, create an infinite loop
                            //std::cerr << "Warning: Infinite loop detected in jump. Skipping jump instruction." << std::endl;
                        }
                        
                        break;
                    }
                    case CALL: {
                        uint32_t callOffset = read24();
                        onEvent();
                        callStack.push({curOffset}); // Save the return address (next instruction after the call)
                        curOffset = callOffset;
                        break;
                    }
                    case RET: {
                        if (!callStack.empty()) {
                            onEvent();
                            curOffset = callStack.top().retOffset;
                            callStack.pop(); // Pop the return address from the call stack
                        }
                        break;
                    }
                    case J2_SET_BANK: {
                        // Banks are setup with setProgram, the BMS versions are discarded.
                        //uint8_t bank = hexData[curOffset++];
                        curOffset += 1;
                        onEvent();
                        break;
                    }
                    case J2_SET_PROG: {
                        uint8_t prog = hexData[curOffset++];
                        onEvent();
                        // Only run program if it isn't followed up by another program change
                        if (hexData[curOffset] != J2_SET_PROG) {
                            setProgram(prog);
                        }
                        break;
                    }
                    case J2_SET_PERF_8: {
                        uint8_t type = hexData[curOffset++];
                        int8_t value = static_cast<int8_t>(hexData[curOffset++]);
                        setEffect(type,value);
                        break;
                    }
                    case J2_SET_PERF_16: {
                        uint8_t type = hexData[curOffset++];
                        int16_t value = static_cast<int16_t>(read16());
                        setEffect(type,value);
                        break;
                    }
                    case NOTE_TRACK:
                        curOffset += 2;
                        onEvent();
                        break;
                    case FIN:
                        onEvent();
                        return;
                    case J2_SET_ARTIC: {
                        uint8_t type = hexData[curOffset++];
                        if (type == 0x62) {
                            uint16_t eventPPQN = read16();
                            ppqn = eventPPQN;
                        } else {
                            curOffset += 2;
                        }
                        break;
                    }
                    case J2_TEMPO: {
                        uint16_t bpm = read16();
                        setTempo(bpm);
                        onEvent();
                        break;
                    }
                    case OPEN_TRACK: {
                        curOffset += 4;
                        break;
                    }

                    /* Below this is thought to be used for ingame events (if boss stunned -> heroic_part) 
                    No loss of quality has been seen in midi files due to their absence.
                    Not fully working as of right now.
                    */

                    // case NAME_CHECK: {
                    //     // Skip bytes until a 0x00 is encountered
                    //     while (isValidOffset() && hexData[curOffset++] != 0x00) {std::cout << std::hex << hexData[curOffset-1] << std::endl;}
                    //     // When one is encountered, keep skipping until the byte isn't 0x00
                    //     while (isValidOffset() && hexData[curOffset] == 0x00) {
                    //         curOffset++;
                    //     }
                    //     onEvent();
                    //     break;
                    // }
                    // case EB:
                    // case D5: {
                    //     break;
                    // }
                    // case OPEN_TRACK_BROS:
                    // case RET_COND: {
                    //     curOffset += 1;
                    //     break;
                    // }
                    // case NAME_BUS:
                    // case D1: {
                    //     curOffset += 2;
                    //     break;
                    // }
                    // case D9: {
                    //     curOffset += 3;
                    //     break;
                    // }
                    // case CALL_COND:
                    // case JUMP_COND: {
                    //     curOffset += 4;
                    //     break;
                    // }
                    // case FA: {
                    //     curOffset += 5;
                    //     break;
                    // }
                    // case DC: {
                    //     curOffset += 11;
                    //     break;
                    // }
                    // case DA: {
                    //     while (isValidOffset()) {
                    //         uint8_t value = hexData[curOffset];
                    //         bool foundInEnum = false;

                    //         // Check if the value matches any in the MML enum
                    //         for (int i = OPEN_TRACK; i <= JUMP_COND; i++) {
                    //             if (value == static_cast<uint8_t>(i)) {
                    //                 foundInEnum = true;
                    //                 break;
                    //             }
                    //         }

                    //         if (foundInEnum) {
                    //             // The value matches one in the MML enum, so we stop the loop
                    //             break;
                    //         }

                    //         // If the value doesn't match, increment curOffset and continue the outer loop
                    //         curOffset++;
                    //     }
                    //     break;
                    // }

                    default: {
                        if (firstTrack) {
                            firstTrackErrorHandling(status_byte);
                            return;
                        } else {
                            std::cout << "! ERROR: A byte could not be read. !" << std::endl;
                            std::cout << "Track Number: " << static_cast<int>(trackNum) << std::endl;
                            std::cout << "Status Byte: 0x" << std::hex << static_cast<int>(status_byte) << std::endl;
                            std::cout << "Previous Byte: 0x" << std::hex << static_cast<int>(hexData[curOffset -2]) << std::endl;
                            std::cout << "Offset: 0x" << std::hex << static_cast<int>(curOffset) << std::endl;
                            return;
                        }
                    }
                }
            }
        }
    }

    void setEffect(uint8_t type, double value) {
        if (type == MML_VOLUME){
            uint8_t midValue = value;
            setVolume(midValue);
        } else if (type == MML_PITCH){
            uint16_t midValue = value;
            setPitch(midValue);
        }else if (type == MML_REVERB) {
            uint8_t midValue = value;
            setReverb(midValue);
        } else if (type == MML_PAN) {
            uint8_t midValue = value;
            addPan(midValue);
        } else if (type == MML_EFFECT_UNKNOWN) {
            if (value != 0x00) {
                std::cout << "Notice: Encountered an effect parameter of 0x04 that isn't a 0 byte; 0x" << std::hex << static_cast<int>(value) << std::endl;
            }
        } else {
            std::cout << "! ERROR: SetPerf found a unknown byte. !" << std::endl;
            std::cout << "Track Number: " << static_cast<int>(trackNum) << std::endl;
            std::cout << "Byte Type: 0x" << std::hex << static_cast<int>(type) << std::endl;
            std::cout << "Value Byte: 0x" << std::hex << static_cast<int>(value) << std::endl;
            std::cout << "Offset: 0x" << std::hex << static_cast<int>(curOffset) << std::endl;
        }
        onEvent();
    }

    void onEvent() {
        if (trackStartGlob > VisitedAddressMax) {
            VisitedAddressMax = trackStartGlob;
        }
        VisitedAddresses.insert(trackStartGlob);
    }

    bool isOffsetUsed(uint32_t offset) {
        if (curOffset <= VisitedAddressMax) {
            auto itrOffset = VisitedAddresses.find(curOffset);
            if (itrOffset != VisitedAddresses.end()) {
                return true;
            }
        }
        return false;
    }

    uint32_t convertFromVLQ() {
        // Reads it as VLQ (Variable-length quantity), so following calculations can work with correct values
        register uint32_t value;
        register uint8_t c;

        if (isValidOffset() && (value = hexData[curOffset++]) & 0x80) {
            value &= 0x7F;
            do {
            if (!isValidOffset())
                break;
            value = (value << 7) + ((c = hexData[curOffset++]) & 0x7F);
            } while (c & 0x80);
        }
        return value;
    }

    std::vector<uint8_t> convertToVLQ(uint32_t input) {
        // Conversion back to VLQ (used for MIDI)
        std::vector<uint8_t> buf;
        uint32_t buffer = input & 0x7F;

        input >>= 7;

        while (input > 0) {
            buffer <<= 8;
            buffer |= ((input & 0x7F) | 0x80);
            input >>= 7;
        }

        while (true) {
            buf.push_back(static_cast<uint8_t>(buffer));
            if (buffer & 0x80)
                buffer >>= 8;
            else
                break;
        }
        return buf;
    }

    bool isValidOffset() {
        return (curOffset < hexData.size());
    }

    uint16_t read16() {
        if (curOffset + 1 >= hexData.size()) {
            throw std::out_of_range("Offset is out of bounds");
        }
        uint16_t value = (static_cast<uint16_t>(hexData[curOffset]) << 8) | static_cast<uint16_t>(hexData[curOffset + 1]);
        curOffset += 2;
        return value;
    }

    uint32_t read24() {
        if (curOffset + 2 >= hexData.size()) {
            throw std::out_of_range("Offset is out of bounds");
        }

        uint32_t value = (static_cast<uint32_t>(hexData[curOffset]) << 16) |
                         (static_cast<uint32_t>(hexData[curOffset + 1]) << 8) |
                         static_cast<uint32_t>(hexData[curOffset + 2]);
        curOffset += 3;
        return value;
    }

    uint32_t getWord(uint32_t nIndex) {
        assert((nIndex >= 0) && (nIndex + 4 <= hexData.size()));
        return ((static_cast<uint32_t>(hexData[nIndex]) << 24) +
                (static_cast<uint32_t>(hexData[nIndex + 1]) << 16) +
                (static_cast<uint32_t>(hexData[nIndex + 2]) << 8) +
                static_cast<uint32_t>(hexData[nIndex + 3]));
    }

    bool addedStartingTrackStart = false;

    void scanForTracks(uint32_t offset) {
        if (!addedStartingTrackStart) {
            trackList.push_back(std::make_tuple(0, 0, 0));
            addedStartingTrackStart = true;
        }
        while (hexData[offset] == OPEN_TRACK) {
            uint8_t trackNo = hexData[offset + 1];
            uint32_t trackStart = getWord(offset + 1) & 0x00FFFFFF;
            scanForTracks(trackStart);
            trackList.push_back(std::make_tuple(trackNo + 1, trackStart, 0));
            offset += 0x05;
        }
    }

    void getTrackPointers() {
        scanForTracks(0);

        if (trackList.empty()) {
            return;
        }

        // Set the first track's end to the "last track" (2nd),
        std::get<2>(trackList.front()) = std::get<1>(trackList.back());
        // Remove the "last track" (scanned along with first track)
        trackList.pop_back(); 

        for (size_t i = 1; i < trackList.size(); i++) {
            uint32_t nextTrackStart;
            if (i < trackList.size() - 1) {
                nextTrackStart = std::get<1>(trackList[i + 1]);
            } else {
                nextTrackStart = hexData.size();
            }
            std::get<2>(trackList[i]) = nextTrackStart;
        }
    }

    void firstTrackErrorHandling(uint8_t status_byte) {
            std::cout << "Notice: A byte could not be read on the inital track." << std::endl;
            std::cout << "File will still be converted, inital track bytes is yet to be deciphered." << std::endl;
            std::cout << "Status Byte: 0x" << std::hex << static_cast<int>(status_byte) << std::endl;
            std::cout << "Offset: 0x" << std::hex << static_cast<int>(curOffset) << std::endl;
    }

    /*Midi Creation*/

    std::ofstream outputFile;
    std::vector<unsigned char> midiData;
    uint32_t accumulatedWaitTime = 0;
    uint32_t previousEventTimestamp = 0;

    std::vector<std::tuple<uint8_t, uint8_t, bool>> midiMappings; // Status Num, Program, Has pitch changes (occupied)
    int currentMidiMapping;
    uint8_t statusNum = 0x00;

    void writeMIDIData(const std::vector<unsigned char>& eventData) {
        midiData.insert(midiData.end(), eventData.begin(), eventData.end());
    }

    void writeMIDIData(const std::vector<unsigned char>& eventData, std::size_t position) {
        midiData.insert(midiData.begin() + position, eventData.begin(), eventData.end());
    }

    void finalizeMIDIFile() {
        outputFile.write(reinterpret_cast<const char*>(midiData.data()), midiData.size());
    }

    size_t trackStartMarker = 0;
    
    void handleTrackPoints() {
        // Write the track end
        std::vector<unsigned char> trackEnd = {0x00, 0xFF, 0x2F, 0x00};
        writeMIDIData(trackEnd);

        // MIDI track header (length accounts for track end)
        std::vector<unsigned char> trackHeader = {
            'M', 'T', 'r', 'k',
            static_cast<unsigned char>((midiData.size() - trackStartMarker) >> 24 & 0xFF),
            static_cast<unsigned char>((midiData.size() - trackStartMarker) >> 16 & 0xFF),
            static_cast<unsigned char>((midiData.size() - trackStartMarker) >> 8 & 0xFF),
            static_cast<unsigned char>((midiData.size() - trackStartMarker) & 0xFF),
        };

        writeMIDIData(trackHeader, trackStartMarker);
    }

    void handleMIDIHeader() {
        std::vector<unsigned char> header = {
            'M', 'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, static_cast<unsigned char>(trackList.size())
        };

        unsigned char* ppqnBytes = reinterpret_cast<unsigned char*>(&ppqn);

        header.push_back(ppqnBytes[1]);
        header.push_back(ppqnBytes[0]);

        writeMIDIData(header, 0);
    }

    std::vector<uint8_t> calculateDeltaTime() {
        uint32_t deltaTime = accumulatedWaitTime - previousEventTimestamp;
        previousEventTimestamp = accumulatedWaitTime; // Update timestamp
        return convertToVLQ(deltaTime);
    }

    void setProgram(uint8_t program) {
        uint8_t bank = program / 128;
        uint8_t actualProgram = program - 128 * bank;
        bank += 0x16;

        // Assumed that program select is always first in the track
        // Check if the MIDI mapping already exists in the list
        bool mappingExists = false;
        uint8_t existingStatusNum = 0x00;

        for (const auto& mapping : midiMappings) {
            if (std::get<1>(mapping) == program && std::get<2>(mapping) == false) {
                mappingExists = true;
                existingStatusNum = std::get<0>(mapping);
                break;
            }
        }

        if (!mappingExists) {
            // Determine the new statusNum based on the last statusNum in the vector
            uint8_t newStatusNum = midiMappings.empty() ? 0x00 : (std::get<0>(midiMappings.back()) + 1);

            // Add the MIDI mapping to the global list
            midiMappings.push_back(std::make_tuple(newStatusNum, program, false));

            // Use the newly determined statusNum
            statusNum = newStatusNum;
        } else {
            // Use the existing statusNum from the list
            statusNum = existingStatusNum;
        }

        trackInstruments.push_back(std::make_tuple(trackNum, program));

        // MIDI bank select event
        std::vector<unsigned char> bankSelectEvent = calculateDeltaTime();
        bankSelectEvent.push_back(0xB0 + statusNum);
        bankSelectEvent.push_back(0x00);
        bankSelectEvent.push_back(bank);

        // MIDI program change event
        std::vector<unsigned char> programChangeEvent = calculateDeltaTime();
        programChangeEvent.push_back(0xC0 + statusNum);
        programChangeEvent.push_back(actualProgram);

        writeMIDIData(bankSelectEvent);
        writeMIDIData(programChangeEvent);
    }

    void handleNoteOn(uint8_t note, uint8_t velocity) {

        // Find if the same note is already being played
        for (uint8_t voice = 0; voice < 8; ++voice) {
            if (voiceToNote[voice] == note) {
                // Same note is already being played, trigger note-off event
                handleNoteOff(voice + 1);
                break;
            }
        }

        // Find an available voice ID
        uint8_t voice = 0;
        for (; voice < 8; ++voice) {
            if (voiceToNote[voice] == 0) {
                // Available voice ID found, assign the note
                voiceToNote[voice] = note;
                break;
            }
        }

        // Check if an available voice ID was found
        if (voice < 8) {
            // Create MIDI note-on event
            uint8_t statusByte = 0x90 + statusNum;

            // Create the MIDI note-on event data
            std::vector<unsigned char> eventData = calculateDeltaTime();  // Delta time
            eventData.push_back(statusByte);
            eventData.push_back(note);
            eventData.push_back(velocity);

            writeMIDIData(eventData);
        }
    }

    void handleNoteOff(uint8_t voice) {

        // Validate voice ID
        if (voice > 0 && voice <= 8) {
            // Retrieve the note being played by the specified voice
            uint8_t note = voiceToNote[voice - 1];

            // Reset the voice ID to indicate it's available
            voiceToNote[voice - 1] = 0;

            // Create MIDI note-off event
            uint8_t statusByte = 0x80 + statusNum;

            // Create the MIDI note-off event data
            std::vector<unsigned char> eventData = calculateDeltaTime();  // Delta time
            eventData.push_back(statusByte);
            eventData.push_back(note);
            eventData.push_back(0x40);  // Velocity is set to 0 for note-off

            writeMIDIData(eventData);
        }
    }


    void turnOffRemainingNotes() {
        std::vector<unsigned char> eventData = calculateDeltaTime();  // Delta time
        eventData.push_back(0xB0 + statusNum);
        eventData.push_back(0x7B);
        eventData.push_back(0x00);  // Velocity is set to 0 for note-off

        writeMIDIData(eventData);
    }

    void addTime(uint32_t time) {
        accumulatedWaitTime += time;
    }

    void setTempo(uint16_t bpm) {
        // Calculate the tempo value in microseconds per quarter note (MPQN)
        uint32_t microsecondsPerQuarterNote = static_cast<uint32_t>(60000000 / bpm);
        tempo = microsecondsPerQuarterNote;

        // MIDI meta event for setting tempo
        std::vector<unsigned char> tempoEvent = calculateDeltaTime();

        // Add the tempo event data
        tempoEvent.push_back(0xFF);
        tempoEvent.push_back(0x51);
        tempoEvent.push_back(0x03);
        tempoEvent.push_back(static_cast<unsigned char>((microsecondsPerQuarterNote >> 16) & 0xFF));
        tempoEvent.push_back(static_cast<unsigned char>((microsecondsPerQuarterNote >> 8) & 0xFF));
        tempoEvent.push_back(static_cast<unsigned char>(microsecondsPerQuarterNote & 0xFF));

        writeMIDIData(tempoEvent);
    }

    void setVolume(uint8_t volume) {
        // MIDI control change event for volume
        std::vector<unsigned char> volumeEvent = calculateDeltaTime();
        volumeEvent.push_back(0xB0 + statusNum);
        volumeEvent.push_back(0x07);
        volumeEvent.push_back(volume);

        writeMIDIData(volumeEvent);
    }

    bool isPitchSetup = false;

    void setPitch(int16_t pitch) {

        if (!isPitchSetup) {
            /* Not too sure if other games BMS files require a pitch adjustment, but the TP soundfont does. */
            uint8_t statusByte = 0xB0 + statusNum;

            std::vector<unsigned char> pitchSetup = calculateDeltaTime();
            pitchSetup.insert(pitchSetup.end(), {
                statusByte, 0x64, 0x00,               // Pitch coarse init
                0x00, statusByte, 0x65, 0x00,         // Pitch fine init
                0x00, statusByte, 0x06, 0x30,         // Pitch course +30 semitones
                0x00, statusByte, 0x26, 0x00,         // Pitch fine   +0 cents
                0x00, statusByte, 0x64, 0x7f,         // Pitch course end
                0x00, statusByte, 0x65, 0x7f          // pitch fine end
                });

            // Set the statusNum to be 'occupied'
            for (auto& mapping : midiMappings) {
                if (std::get<0>(mapping) == statusNum) {
                    // Set the bool value to true for the found mapping
                    std::get<2>(mapping) = true;
                    break;
                }
            }

            writeMIDIData(pitchSetup);
            isPitchSetup = true;
        }

        const int16_t midiMidpoint = 0x2000; // MIDI pitch bend midpoint (16384 / 2)

        // Convert the input pitch to MIDI pitch bend range (0x0000 to 0x3FFF)
        int16_t midiPitch = pitch / 4;

        // Check if the pitch is above the MIDI pitch bend midpoint, deal with inversion
        if (midiPitch > midiMidpoint) {
            midiPitch -= midiMidpoint; // Subtract the midpoint for pitch-up
        } else {
            midiPitch += midiMidpoint; // Add the midpoint for pitch-down
        }

        // Extract the lower 7 bits and upper 7 bits from the converted pitch value
        uint8_t lsb = static_cast<uint8_t>(midiPitch & 0x7F);
        uint8_t msb = static_cast<uint8_t>((midiPitch >> 7) & 0x7F);

        std::vector<unsigned char> pitchEvent = calculateDeltaTime();
        pitchEvent.push_back(0xE0 + statusNum);
        pitchEvent.push_back(lsb);       // Pitch bend LSB
        pitchEvent.push_back(msb);       // Pitch bend MSB

        writeMIDIData(pitchEvent);
    }

    void setReverb(uint8_t value) {
        // MIDI control change event for reverb (not sustain)
        std::vector<unsigned char> reverbEvent = calculateDeltaTime();
        reverbEvent.push_back(0xB0 + statusNum);
        reverbEvent.push_back(0x5B);
        reverbEvent.push_back(value);

        writeMIDIData(reverbEvent);
    }

    void addPan(uint8_t pan) {
        // MIDI control change event for pan
        std::vector<unsigned char> panEvent = calculateDeltaTime();
        panEvent.push_back(0xB0 + statusNum);
        panEvent.push_back(0x0A);
        panEvent.push_back(pan);

        writeMIDIData(panEvent);
    }

    void trackReset() {
        //Basics to reset variables for new track
        accumulatedWaitTime = 0;
        previousEventTimestamp = 0;
        VisitedAddresses.clear();
        VisitedAddresses.reserve(8192);
        VisitedAddressMax = 0;
        trackStartMarker = midiData.size();
        isPitchSetup = false;
        firstTrack = false;
    }

    /*Main Run*/

    void init() {

        getTrackPointers();

        // std::cout << "Track List:" << std::endl;
        // for (const auto& track : trackList) {
        //     std::cout << "Track No: " << static_cast<int>(std::get<0>(track))
        //               << ", Track Start: " << static_cast<int>(std::get<1>(track))
        //               << ", Track End: " << static_cast<int>(std::get<2>(track)) << std::endl;
        // }

        for (const auto& track : trackList) {
            // Makes hexcode neater, but also prevents track 0's error code being 255
            trackNum = (std::get<0>(track) == 0x00) ? std::get<0>(track) : (std::get<0>(track) - 1);
            uint32_t trackStart = std::get<1>(track);
            uint32_t trackEnd = std::get<2>(track);
            parseEvents(trackStart, trackEnd);
            turnOffRemainingNotes();
            handleTrackPoints();
            trackReset();
        }

        handleMIDIHeader(); // Add header
        finalizeMIDIFile();
        std::cout << "BMS file converted" << std::endl;

    }

    void printTrackInstruments() {
        for (const auto& instrument : trackInstruments) {
            uint8_t trackNum = std::get<0>(instrument);
            uint8_t program = std::get<1>(instrument);
            std::cout << "TrackNum: " << std::dec << static_cast<int>(trackNum) << ", Program: " << std::dec << static_cast<int>(program) << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename> [--instruments]" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    std::string midiFilename = filename.substr(0, filename.find_last_of('.')) + ".mid";

    std::ifstream inputFile(filename, std::ios::binary);
    if (!inputFile) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return 1;
    }

    std::ofstream outputFile(midiFilename, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Failed to create MIDI file: " << midiFilename << std::endl;
        return 1;
    }

    std::vector<unsigned char> hexData(
        (std::istreambuf_iterator<char>(inputFile)),
        (std::istreambuf_iterator<char>()));

    while (!hexData.empty() && hexData.back() == 0x00) {
        hexData.pop_back();
    }

    TrackParser parser;
    parser.hexData = hexData;
    parser.outputFile = std::move(outputFile);
    parser.init();

    // Check if the --instruments argument is present
    if (argc >= 3 && std::string(argv[2]) == "--instruments") {
        // Call the printTrackInstruments function
        std::cout << "Track Instruments:" << std::endl;
        parser.printTrackInstruments();
    }

    return 0;
}
