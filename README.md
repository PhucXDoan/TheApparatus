# The Apparatus.

The Apparatus supersedes [The Machine](https://github.com/PhucXDoan/TheMachine). I decided to do the entire project again, because 1. typing in the letters of the wordgame sucked and 2. using the Arduino USB HID library sucked even more. So just under the span of four months, I took the oath of celibacy from all external dependencies and rewrote the project all entirely from scratch.

This includes writing myself the USB firmware stack, which implements the mass storage interface to **automatically perform OCR on the fly**. It's pretty sick, honestly. All of it was done entirely on the ATmega32U4, an 8-bit AVR microcontroller running at 16 MIPS. To achieve this, I took the approach of using the cutting edge artifical intelligence algorithm that is: comparing BMPs of the letters against the screenshot of the game and picking the close matching one. It ain't slow either (lots of clever optimizations had to be done). It only takes around 6 seconds to parse the board of the largest word game 🚀🚀🚀🚀🚀!

Here are some demos:

## **Anagrams, 7 Letters, 78 words, 41900 points in 60 seconds.**

https://github.com/PhucXDoan/TheApparatus/assets/77856521/72c686d9-0118-4c60-aa85-98ccec668764

## **Anagrams, Russian, 26 words, 9700 points in 38 seconds.**

The Apparatus is not particularly fluent in Russian which wouldn't be the case if I had a better corpus. Probably still better than you, though. 

https://github.com/PhucXDoan/TheApparatus/assets/77856521/4cc8f278-cac8-49cf-88b2-d9d563769210

## **Anagrams, Spanish, 45 words, 29000 points in 36 seconds.**

At least it knows Spanish!

https://github.com/PhucXDoan/TheApparatus/assets/77856521/f1a40c71-589b-41ae-b2b7-9320400d9329

## **Word Hunt, 5x5, 215 words, 95700 points in 30 seconds.**

https://github.com/PhucXDoan/TheApparatus/assets/77856521/5d9027a3-db13-4c70-94aa-0132747711ff

## **Word Bites, 161 words, 72300 points in 80 seconds.**

The Apparatus can now even do Word Bites, a word game that The Machine didn't do before.

https://github.com/PhucXDoan/TheApparatus/assets/77856521/101973ef-90ce-4dfc-977b-a8b7abb88078

# Other Notes

The Apparatus mostly uses the same design that of The Machine where it is comprised of two 8-bit AVR microcontrollers: an ATmega32U4 and a ATmega2560. The ATmega32U4 (which I call *The Diplomat*, because I'm not great with names) handles all the USB transactions (including the OCR) using only 2.5KB of RAM. The ATmega2560 (*The Nerd*) handles all the processing of the words; having two MCUs allows for multi-tasking of sorts, between handling USB transactions and processing for reproducibility of words. I used these microcontrollers because... well, they're just what I'm happened to be familiar with and had around. I probably could've used some beefy 32-bit microcontroller and have an easier time, but where's the fun in that?

The Apparatus relies on a dataset for the words in English, Russian, French, German, Spanish, and Italian. Several gigabytes worth of screenshots of each word game are also needed to generate masks for the OCR.
