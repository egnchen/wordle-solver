# Wordle solver

Solver for the [wordle game](https://www.nytimes.com/games/wordle/index.html) inspired by this [3blue1brown video](https://www.youtube.com/watch?v=v68zYyaEmEA&t=1031s).

Current performance is ~3.47 guesses on average:
```
Distribution:
1       0
2       45
3       1208
4       992
5       63
6       1
>6      0
Average =       3.466
```

Implementation details which are different from the video:
* Lack of heuristic function. Essentially this is the "version one" described in the video.
    * After experiment two-level heuristic is an overkill. Its improvement is marginal and would drastically hurt performance.

## How to use?

Make sure you have a C++14 compliant compiler, then use `make` to build it.

The way to describe wordle output:
* **c** means correct;
* **p** means position not correct;
* **n** means not found.

For example, if the first character is correct(green), second's position is wrong(yellow) and following is not found(grey), you should input: `cpnnn` when asked for result.
