# Issue Guidelines

First of all, please remember to:
- Check that your issue is not a duplicate
- Read the [FAQ](https://github.com/AbyssWM/Abyssland)
- Read the [Configuring Page](https://github.com/AbyssWM/Abyssland)

<br/>

# Reporting suggestions
Suggestions are welcome.

Many features can be implemented using bash scripts and Abyssland sockets, read up on those [Here](https://github.com/AbyssWM/Abyssland). Please do not suggest features that can be implemented as such.

<br/>

# Reporting bugs

All bug reports should have the following:
- Steps to reproduce
- Expected outcome
- Noted outcome

If your bug is one that doesn't crash Abyssland, but feels like invalid behavior, that's all you need to say.

If your bug crashes Abyssland, append additionally:
- The Abyssland log
- Your config
- (v0.22.0beta and up) The Abyssland Crash Report
- (v0.21.0beta and below) Coredump / Coredump analysis (with a stacktrace)

**Important**: Please do NOT use any package for reporting bugs! Clone and compile from source.

## Obtaining the Abyssland log
If you are in a TTY, and the abyssland session that crashed was the last one you launched, the log will be printed with
```
cat $XDG_RUNTIME_DIR/abyss/$(ls -t $XDG_RUNTIME_DIR/abyss | head -n 1)/abyssland.log
```
feel free to send it to a file, save, copy, etc.

if you are in a Abyssland session, and you want the log of the last session, use
```
cat $XDG_RUNTIME_DIR/abyss/$(ls -t $XDG_RUNTIME_DIR/abyss | head -n 2 | tail -n 1)/abyssland.log
```

basically, directories in $XDG_RUNTIME_DIR/abyss are your sessions.

## Obtaining the Abyssland Crash Report (v0.22.0beta and up)

If you have `$XDG_CACHE_HOME` set, the crash report directory is `$XDG_CACHE_HOME/abyssland`. If not, it's `$HOME/.cache/abyssland`.

Go to the crash report directory and you should find a file named `abysslandCrashReport[XXXX].txt` where `[XXXX]` is the PID of the process that crashed.

Attach that file to your issue.
## Obtaining the Abyssland coredump (v0.21.0beta and below)
If you are on systemd, you can simply use
```
coredumpctl
```
then go to the end (press END on your keyboard) and remember the PID of the last `Abyssland` occurrence. It's the first number after the time, for example `2891`.

exit coredumpctl (ctrl+c) and use
```
coredumpctl info [PID]
```
where `[PID]` is the PID you remembered.

## Obtaining the debug Abyssland coredump
A debug coredump provides more information for debugging and may speed up the process of fixing the bug.

Make sure you're on latest git. Run `git pull --recurse-submodules` to sync everything.

1. [Compile Abyssland with debug mode](https://github.com/AbyssWM/Abyssland)
> Note: The config file used will be `abysslandd.lua` instead of `abyssland.lua`

2. `cd ~`
3. For your own convenience, launch Abyssland from a tty with the envvar `ASAN_OPTIONS="log_path=asan.log" ~/path/to/Abyssland`
4. Reproduce the crash. Abyssland should instantly close.
5. Check out your `~` and find a file called `asan.log.XXXXX` where `XXXXX` will be a number corresponding to the PID of the Abyssland instance that crashed.
6. That is your coredump. Attach it to your issue.
