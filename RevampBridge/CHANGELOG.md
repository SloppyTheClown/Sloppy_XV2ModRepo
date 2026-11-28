# Changelog

## 1.0.1
* Fewer antivirus false positives: the exe now has file version details and
  an application manifest, the patch set ships as plain files in
  `Revamp Bridge Files\setup\` instead of inside the exe, and the program run
  under the installer's name is shipped in `Revamp Bridge Files\` instead of
  being copied there at run time. No change to what gets installed.

## 1.0.0
* First release: `Revamp Bridge.exe` installs Revamp Xenoverse 2 5.1.1 on
  game 1.27 (and 1.26.01) from the player's own Revamp download (the zip or
  its extracted contents). The release carries no file of Revamp's; it
  rebuilds the patched installer library and the ported exe patches from it.
