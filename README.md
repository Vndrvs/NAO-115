# Nao-limit Poker Engine

* This is a work in progress poker engine written in C++.
* It will be modular and fast as lightning. 
* I'm aiming for reaching the MIT Pokerbots standard of 1000 hands / 30 seconds.

## Project goals

* Get and encode card data (preflop-river)
* Evaluate all hands and board states
* Implement MCCFR to support abstraction buckets and decision trees as well
* Add analysis and UI control tools

## Main principles

* Readable and commented code
* Builded step by step, from encoders to finished CFR based actions

## Styling and directory

* Currently, the structuring of the project is not a final version, I want to save time, therefore I'll finalize the hierarchy after several more modules are added.
* I'm trying to use CPP naming conventions, unless it hinders my proper understanding and / or ability to read the code

## References and Acknowledgments

This project builds upon several well-known and highly respected works:

* 5-card hand evaluation is based on Kevin Suffecool's (Cactus Kev) poker hand evaluator:
  http://suffe.cool/poker/evaluator.html

* The perfect hashing improvements introduced by Paul Senzee are used to optimize hand ranking:
  https://senzee.blogspot.com/2006/06/some-perfect-hash.html

  The evaluator has been extended to support 6- and 7-card hands using custom bitmasking and structural modifications.

* Eigenvalue and eigenvector computations for symmetric matrices (used in hand bucketing analysis) rely on Andrew Jewett’s implementation of the Jacobi algorithm:
  https://github.com/jewettaij/jacobi_pd/tree/master
