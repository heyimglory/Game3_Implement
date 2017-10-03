# Hung-yu's implementation of [Spin](http://graphics.cs.cmu.edu/courses/15-466-f17/game3-designs/hungyuc/) for game3 in 15-466-f17

![alt text](https://github.com/heyimglory/Game3_Implement/blob/master/screenshots/start.png)

## Asset Pipeline

The asset pipeline of this game is based on export-meshes.py provided in Base2. It reads out the data of the vertices from a blender file and writes them into binary blob files, which are the data of the meshes and the data of the scene. The blob files can be read by the main program directly because their format are adjusted to meet the loading functions. The part regarding the color of the verices is referencing export-layer.py.

## Architecture

The first step of the program is to load the meshes and set up the scene. After that, in the game loop, the user input that affects the translation and the spinning direction of the spinning stuff is first handled, followed by updating the status of the rotation, the normal, and whether it collides with the ball. Then, the status of the ball, including the friction, whether it collides into the walls or the pillars, is updated. Finally, according to the position of the ball, whether any of the player wins is determined.

## Reflection

I think the most difficult part of this assignment is to determine the collisions. I feel that my way of detecting them is still kind of approxiamte and brute. Maybe I would try to make the computation more consise if I want to refine it. Also, in the current implementation, the ball sometimes still gets through the plane of the spinning stuff if they collide while the spinning stuff is moving. This is definitely not following the basic physic rules. However, if the ball never gets through the plane, the players can just keep hitting the key that changes the spinning direction to make it fixed and simply push the ball all the way to the other side. This situation could be further clarified in the design document.

# About Base2

This game is based on Base2, starter code for game2 in the 15-466-f17 course. It was developed by Jim McCann, and is released into the public domain.

## Requirements

 - modern C++ compiler
 - glm
 - libSDL2
 - libpng
 - blender (for mesh export script)

On Linux or OSX these requirements should be available from your package manager without too much hassle.

## Building

This code has been set up to be built with [FT jam](https://www.freetype.org/jam/).

### Getting Jam

For more information on Jam, see the [Jam Documentation](https://www.perforce.com/documentation/jam-documentation) page at Perforce, which includes both reference documentation and a getting started guide.

On unixish OSs, Jam is available from your package manager:
```
	brew install ftjam #on OSX
	apt get ftjam #on Debian-ish Linux
```

On Windows, you can get a binary [from sourceforge](https://sourceforge.net/projects/freetype/files/ftjam/2.5.2/ftjam-2.5.2-win32.zip/download),
and put it somewhere in your `%PATH%`.
(Possibly: also set the `JAM_TOOLSET` variable to `VISUALC`.)

### Bulding
Open a terminal (on windows, a Visual Studio Command Prompt), change to this directory, and type:
```
	jam
```

### Building (local libs)

Depending on your OSX, clone 
[kit-libs-linux](https://github.com/ixchow/kit-libs-linux),
[kit-libs-osx](https://github.com/ixchow/kit-libs-osx),
or [kit-libs-win](https://github.com/ixchow/kit-libs-win)
as a subdirectory of the current directory.

The Jamfile sets up library and header search paths such that local libraries will be preferred over system libraries.
