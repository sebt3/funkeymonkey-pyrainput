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
<parameter> = <value>

mouse.sensitivity	= <int>
mouse.deadzone		= <int>
mouse.wheel.deadzone	= <int>
mouse.click.deadzone	= <int>
nubs.left.x		= [mouse_x|mouse_y|mouse_btn|scroll_x|scroll_y]
nubs.left.y		= [mouse_x|mouse_y|mouse_btn|scroll_x|scroll_y]
nubs.right.x		= [mouse_x|mouse_y|mouse_btn|scroll_x|scroll_y]
nubs.right.y		= [mouse_x|mouse_y|mouse_btn|scroll_x|scroll_y]
