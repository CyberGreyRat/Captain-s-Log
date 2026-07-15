# Captain Web v0.6 Archive Workflow

Archiving replaces deletion. Requirements keep their YAML and Git history by
using `status: Archived`. Document links remain in SQLite and use an internal
`archived:` kind prefix. Both support restoration and generate web audit events.

The GUI adds active tree, archive requirement view, active/archive documents,
Tree and Status workflow actions.
