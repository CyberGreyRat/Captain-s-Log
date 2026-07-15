# Captain Web v0.8 Repair

This repair handles the observed mixed source state:

- v0.7 header/main with AuthService and ProjectCatalog;
- older five-argument ApiServer constructor in api_server.cpp;
- dynamic form attributes not yet applied;
- local YAML attributes parser/writer still incomplete.

It repairs the constructor, applies dynamic CLASS/RISK/SRS/SEC/test forms, and
installs the YAML attributes parser/writer. Then rebuild Captain Web.
