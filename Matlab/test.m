frequency = 28; % For example, the frequency of middle C
note = frequencyToNote(frequency);
disp(['The note is: ', note]);

function note = frequencyToNote(frequency)
    % Define the standard tuning frequency (A4)
    A4 = 440;

    % Calculate the note index
    noteIndex = round(12 * log2(frequency / A4) + 49);

    % Define the mapping of note indices to note names
    noteNames = {'A', 'A#', 'B', 'C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#'};

    % Calculate the octave and note within the octave
    octave = floor((noteIndex - 1) / 12);
    noteWithinOctave = mod(noteIndex - 1, 12) + 1;
    if noteWithinOctave > 3
        octave = octave +1 ;
    end
    % Get the note name
    note = [noteNames{noteWithinOctave}, num2str(octave)];
end