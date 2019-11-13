For running our program an example command block is as follows:
rmmod ./mmind.ko
rm /dev/mmind
make
insmod ./mmind.ko mmind_number="4416" mmind_max_guesses=8
mknod /dev/mmind c 250 0
echo "4423" > /dev/mmind
cat /dev/mmind
echo "4425" > /dev/mmind
cat /dev/mmind
gcc test.c -o test
./test

We have also created a test.c file that provides user the 3 options of the game: NEWGAME, ENDGAME, and REMAINING.
Depending on the value user enters to the terminal, it either starts a new game, or shows the remaining guesses, or ends the game.
If the user tries to use the echo command for more than the set max_guesses, (value for max_guesses is either chosen from its default value 10, or given as a module_param)
the game gives error.
If user chooses NEWGAME, the game asks for a new secret number.
If REMAINING is chosen, the game shows the amount of remaining guesses user can make depending on the difference of max_guesses and guesses that have been made up to that point.
If ENDGAME is chosen, the game terminates.

Before starting a new game previous game must be ended first(choose ENDGAME before NEWGAME)
In order to do these, test.c must be compiled an run as follows:
gcc test.c -o test
./test