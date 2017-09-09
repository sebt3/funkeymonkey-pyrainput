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
mouse.sensitivity	= 40
mouse.deadzone		= 20
mouse.wheel.deadzone	= 100
mouse.click.deadzone	= 100
nubs.left.x		= [*mouse_x*|mouse_y|mouse_btn|scroll_x|scroll_y]
nubs.left.y		= [mouse_x|*mouse_y*|mouse_btn|scroll_x|scroll_y]
nubs.right.x		= [mouse_x|mouse_y|*mouse_btn*|scroll_x|scroll_y]
nubs.right.y		= [mouse_x|mouse_y|mouse_btn|scroll_x|*scroll_y*]
nubs.left.click		= [*mouse_left*|mouse_right]
nubs.right.click	= [mouse_left|*mouse_right*]
gamepad.export		= 1
keypad.export		= 1
mouse.export		= 1
```

Then reload configuration with:
```
systemctl reload pyrainput
```
