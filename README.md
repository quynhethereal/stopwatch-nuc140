# Embedded Design 2 - EEET2481
An embedded stopwatch implementation running on NUC140 board. 

# Functional Requirements
The behavior of this stopwatch is: 

- After system reset, LED5 will turn on to indicate Idle mode.
- Pressing K1 (Start/Stop key) will start counting (Count mode).
  - LED5 turns off and LED6 turns on.
- Timer0 interrupt is used to implement the counter with a precision of 1/10 second.
- The elapsed time is displayed on the 4 x 7-segment LEDs.
- Pressing K1 during Count mode will pause the counter (Pause mode).
  - LED6 turns off and LED7 turns on.
  - The display freezes during pause.
- Pressing K9 (Lap/Reset key) during Count mode will record and display lap time on the second 4 x 7-segment LEDs.
  - Maximum 5 lap times can be recorded, and new lap times will overwrite previous ones if K9 is pressed more than 5 times.
- Pressing K9 during Pause mode will reset the counter and go back to Idle mode.
- Pressing K5 (Check key) during Pause mode will enter Check mode to display all recorded lap times.
  - Lap time is displayed with 3 digits only, ignoring minute in this mode.
  - The first digit displays the lap number being checked.
  - GPB15 (Rotate key) can be used to rotate between laps with an external interrupt.
- Pressing K5 during Check mode will exit and go back to Pause mode.


## State machine diagram 

<img width="1160" alt="image" src="https://github.com/quynhethereal/stopwatch-nuc140/assets/49337640/21d0f013-73e3-4af0-9679-4f644f8b0fad">

# Demo

