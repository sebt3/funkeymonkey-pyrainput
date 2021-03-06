# Warning
This repo is archived. Please see https://dev.pyra-handheld.com/packages/funkeymonkey-pyrainput for newer developpement
....

# FunKeyMonkey-pyrainput

A pyra input deamon based on FunKey-MonKey


### Install

Standard CMake stuff:

```
mkdir build
cd build
cmake ..
make
make install
```

### Configuration

All configurations should normally goes into /etc/pyrainput.cfg
syntax should be:
`<parameter> = <value>`

```
scripts.brightness.normal	= <path>
scripts.brightness.Fn		= <path>
scripts.brightness.Shift	= <path>
scripts.brightness.FnShift	= <path>
scripts.brightness.Alt		= <path>
scripts.brightness.Ctrl		= <path>
scripts.brightness.AltCtrl	= <path>
scripts.brightness.FnAlt	= <path>
scripts.brightness.FnCtrl	= <path>
scripts.brightness.ShiftAlt	= <path>
scripts.brightness.ShiftCtrl	= <path>
scripts.brightness.FnShiftAlt	= <path>
scripts.brightness.FnShiftCtrl	= <path>
mouse.sensitivity		= 40
mouse.deadzone			= 20
mouse.wheel.deadzone		= 100
mouse.click.deadzone		= 100
nubs.deadzone			= 10
nubs.left.x			= [*mouse_x*|mouse_y|mouse_btn|scroll_x|scroll_y]
nubs.left.y			= [mouse_x|*mouse_y*|mouse_btn|scroll_x|scroll_y]
nubs.right.x			= [mouse_x|mouse_y|*mouse_btn*|scroll_x|scroll_y]
nubs.right.y			= [mouse_x|mouse_y|mouse_btn|scroll_x|*scroll_y*]
nubs.left.click			= [*mouse_left*|mouse_right]
nubs.right.click		= [mouse_left|*mouse_right*]
gamepad.export			= 1
keypad.export			= 1
mouse.export			= 1
```

Then reload configuration with:
```
systemctl reload pyrainput
```

### command lines for dbp packages


```
sudo /usr/sbin/pyrainputctl enable gamepad
sudo /usr/sbin/pyrainputctl disable gamepad

sudo /usr/sbin/pyrainputctl enable keypad
sudo /usr/sbin/pyrainputctl disable keypad

sudo /usr/sbin/pyrainputctl enable mouse
sudo /usr/sbin/pyrainputctl disable mouse
```
