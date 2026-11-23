This directory contains a simple runner for the imposter and tests on some simple client apps.

The goal of the e2e tests is twofold:
1. check that the system works end-to-end
2. check that the imposter is ergonomic to use and identify areas for improvement before a release
    - is it easy to configure?
    - is it flaky?
    - does it have good defaults?
    - does it have a sane probability model, reaching edge-cases and avoiding stalls?
    - does it perform well enough to use in long or otherwise demanding sessions?
