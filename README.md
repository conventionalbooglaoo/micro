# Micro
Micro is an extremely lightweight text editor in C that focuses on minimizing code size while maintaining essential functionality.


**Core Features:**

-   Basic cursor movement (hjkl vim-style navigation)
-   Text insertion and deletion
-   File loading and saving
-   Simple scrolling for larger files
-   Status line showing filename and modification status
-   Raw mode terminal handling

**Optimizations:**

-   Uses a doubly-linked list for efficient line management
-   Minimal memory usage with dynamic allocation only when needed
-   Compact key handling for navigation and commands
-   No external dependencies beyond standard C libraries

**Usage:**

-   Start with `./micro [filename]`
-   Navigation: hjkl (vim-style)
-   Ctrl+S: Save file
-   Ctrl+Q: Quit (prompts if unsaved changes)
-   Enter: Split line
-   Backspace: Delete character

This editor is about 450 lines of code but provides the essential functionality of a text editor. It's designed to be extremely memory efficient while still being usable for real editing tasks.
Core Features:

Basic cursor movement (hjkl vim-style navigation)
Text insertion and deletion
File loading and saving
Simple scrolling for larger files
Status line showing filename and modification status
Raw mode terminal handling
Optimizations:

Uses a doubly-linked list for efficient line management
Minimal memory usage with dynamic allocation only when needed
Compact key handling for navigation and commands
No external dependencies beyond standard C libraries
Usage:

Start with ./micro [filename]
Navigation: hjkl (vim-style)
Ctrl+S: Save file
Ctrl+Q: Quit (prompts if unsaved changes)
Enter: Split line
Backspace: Delete character
This editor is about 450 lines of code but provides the essential functionality of a text editor. It’s designed to be extremely memory efficient while still being usable for real editing tasks.

Markdown 917 bytes 135 words 27 lines Ln 27, Col 193HTML 713 characters 119 words 20 paragraphs
Add StackEdit to your home screen?
