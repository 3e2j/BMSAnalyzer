import os
import subprocess

# Path to the BMS to MIDI converter executable
bms_to_midi_converter_executable = "bmsanalyzer.exe"

# Path to the folder containing the .bms files
bms_folder = os.getcwd()

def convert_bms_to_midi(bms_file):
    # Build the command to run the BMS to MIDI converter
    command = [bms_to_midi_converter_executable, bms_file]

    try:
        # Run the BMS to MIDI converter for the current BMS file
        subprocess.run(command, check=True)
        print(f"Conversion successful for {bms_file}")
    except subprocess.CalledProcessError as e:
        print(f"Error converting {bms_file}: {e}")

def main():
    # Get a list of all files in the folder
    bms_files = [f for f in os.listdir(bms_folder) if f.endswith(".bms")]

    if not bms_files:
        print("No .bms files found in the folder.")
        return

    for bms_file in bms_files:
        bms_file_path = os.path.join(bms_folder, bms_file)
        convert_bms_to_midi(bms_file_path)

if __name__ == "__main__":
    main()
