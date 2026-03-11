# Nao-limit: HUNL Poker AI Research Project

* A C++ research project focused on solving Heads-Up No-Limit (HUNL) Texas Hold'em.
* Built to be modular and fast as lightning, targeting the MIT Pokerbots benchmark of 1,000 hands / 30 seconds.
* This project explores modern Counterfactual Regret Minimization (MCCFR) techniques, advanced card isomorphism, and neural-guided action abstraction.

## Core Research Focus: Neural Dynamic Betting Abstraction (NDBA)
This project experiments with a custom 3-phase architecture to dynamically learn optimal bet-sizing abstractions, rather than relying strictly on fixed human heuristics:
1. **Blueprint Bootstrapping:** Run standard MCCFR on a highly restricted, fixed bet-size abstraction (e.g., 0.5x, 1x, 2x pot) to generate a baseline dummy strategy.
2. **Trajectory-Sampled Action Networks:** Play forward using the blueprint. At random nodes, extract a 95-dimension state and query an Action Network for candidate bet sizes. We fork the game state, run ~1,000 MCCFR subgame iterations on the forks, and update the network weights based on the EV difference.
3. **Learned Abstraction CFR:** Run the final, massive MCCFR training pass using the optimized, neural-generated bet sizes.

## Project Goals & Capabilities
* **State & Card Encoding:** Lightning-fast bitmask evaluation and isomorphic hand indexing (suit-reduction) for minimal RAM footprint.
* **Hand Bucketing:** Precomputed K-Means clustering (EHS & Hand Potential) for Flop and Turn states, with dynamic runtime evaluation for River states.
* **MCCFR Implementation:** External-sampling Monte Carlo CFR supporting millions of infosets with regret-matching.
* **Performance:** Strict cache-line alignment (`alignas(64)`) and flat-array memory management to run full CFR tables entirely within consumer RAM limits (< 8GB).

## Styling and directory

* Currently, the directory structure is iterative. It will be finalized as the neural network and subgame solving modules mature.
* My aim is to use CPP naming conventions, unless it hinders my proper understanding and / or ability to read the code

## References and Acknowledgments
This project stands on the shoulders of several foundational works in poker AI and high-performance C++:

* **RL-CFR Architecture (Foundational Basis):** The core 3-phase neural-guided action abstraction pipeline (NDBA) implemented in this project is directly inspired by the paper *RL-CFR: Improving Action Abstraction for Imperfect Information Extensive-Form Games with Reinforcement Learning* (Li, Fang, & Huang, 2024). Their novel approach to using reinforcement learning to dynamically optimize bet sizes serves as the theoretical backbone of this project. ([View on arXiv](https://arxiv.org/abs/2403.04344))
* **Hand Isomorphism (`hand_index`):** This project utilizes the `hand_index` library developed by Kevin Waugh (Carnegie Mellon University). Waugh's combinatorics perfectly map poker hands to dense isomorphic indices, saving gigabytes of RAM for this implementation. The original work can be found in his 2013 research ([View on GitHub](https://github.com/kdub0/hand-isomorphism)).
* **Fast Hash Maps (`robin_hood`):** Infoset storage utilizes Martin Ankerl's exceptionally fast `robin_hood::unordered_flat_map` (Open Addressing / Linear Probing) to maintain strict cache locality during tree traversals. ([View on GitHub](https://github.com/martinus/robin-hood-hashing))
* **Hand Evaluation (Cactus Kev / Paul Senzee):** 5-card evaluation is based on Kevin Suffecool's Poker Hand Evaluator [Kevin Suffecool's Poker Hand Evaluator](http://suffe.cool/poker/evaluator.html), optimized with Paul Senzee's perfect hashing improvements [Paul Senzee's perfect hashing improvements](https://senzee.blogspot.com/2006/06/some-perfect-hash.html). It has been extended in this project to include bitmasking shortcuts when evaluating 6-and 7 card hands.
* **Matrix Math:** Eigenvalue and eigenvector computations for symmetric matrices (used in hand bucketing outcome analysis) rely on Andrew Jewett’s C++ implementation of the Jacobi algorithm [Andrew Jewett’s C++ implementation of the Jacobi algorithm](https://github.com/jewettaij/jacobi_pd/tree/master).
