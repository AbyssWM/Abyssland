:title: abyssctl(1)
:author: Vaxerski <*https://github.com/vaxerski*>

NAME
====

abyssctl - Utility for controlling parts of Abyssland from a CLI or a script

SYNOPSIS
========

**abyssctl** [*(opt)flags*] [**command**] [*(opt)args*]

DESCRIPTION
===========

**abyssctl** is a utility for controlling some parts of the compositor from a CLI or a script.

CONTROL COMMANDS
================

**dispatch**

    Call a dispatcher with an argument.

    An argument must be present.
    For dispatchers without parameters it can be anything.

    Returns: *ok* on success, and an error message on failure.

    Examples:
        **abyssctl** *dispatch "hl.dsp.exec_cmd('kitty')"*

        **abyssctl** *dispatch "hl.dsp.window.close()"*

**keyword**

    Set a config keyword dynamically.

    This will not work if your config provider is lua (refer to **eval**)

    Returns: *ok* on success, and an error message on failure.

    Examples:
        **abyssctl** *keyword bind SUPER,0,pseudo*

        **abyssctl** *keyword general:border_size 10*

**eval**

    Evaluate arbitrary Lua expressions dynamically when using a Lua-based config provider.

    Returns: *ok* on success, and an error message on failure.

    Examples:
        **abyssctl** *eval 'hl.bind("SUPER + SHIFT + Q", hl.dsp.exec_cmd("firefox"))'*

        **abyssctl** *eval 'hl.config({ general = { border_size = 10 } })'*

**reload**

    Force a reload of the config file.

**kill**

    Enter kill mode, where you can kill an app by clicking on it.
    You can exit by pressing ESCAPE.

INFO COMMANDS
=============

**version**

    Prints the Abyssland version, flags, commit and branch of build.

**monitors**

    Lists all the outputs with their properties.

**workspaces**

    Lists all workspaces with their properties.

**clients**

    Lists all windows with their properties.

**devices**

    Lists all connected input devices.

**activewindow**

    Returns the active window name.

**layers**

    Lists all the layers.

**splash**

    Returns the current random splash.

**status**

    Returns internal status information like config format or backend.

OPTIONS
=======

**--batch**

    Specify a batch of commands to execute.

    Example:
        **abyssctl** *--batch "keyword general:border_size 2 ; keyword general:gaps_out 20"*

        *;* separates the commands.

**-j**

    Outputs information in JSON.

BUGS
====

Submit bug reports and request features online at:
    <*https://github.com/AbyssWM/Abyssland/issues*>

SEE ALSO
========

Sources at: <*https://github.com/AbyssWM/Abyssland*>

COPYRIGHT
=========

Copyright (c) 2022, vaxerski
