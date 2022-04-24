# Wordle solver

Solver for the [wordle game](https://www.nytimes.com/games/wordle/index.html) inspired by this [3blue1brown video](https://www.youtube.com/watch?v=v68zYyaEmEA&t=1031s).

Current performance is ~3.56 guesses on average:
```
.......................
Average =	3.55825
Distribution:
1	1
2	74
3	1074
4	976
5	164
6	19
>6	1
```

Implementation details which are different from the video:
* Lack of heuristic function. Essentially this is the "version one" described in the video.
* Combined use of list of possible answers(~2000 words) with list of possible words(~10000 words). This could drastically improve performance.

## How to use?

Make sure you have a C++17 compliant compiler, then use `make` to build it.

The way to describe wordle output:
* **c** means correct;
* **p** means position not correct;
* **n** means not found.

For example, if the first character is correct(green), second's position is wrong(yellow) and following is not found(grey), you should input: `cpnnn` when asked for result.
