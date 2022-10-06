# UE5-Light-Detector
A C++ Implementation of a stealth game Light Meter similar to those seen in games such as the Thief or Splinter Cell games. 

# Overview
This method does not use the more commonly seen method of raytcasting from the player to light sources. Instead, it takes a RenderTexture of an Octahedron mesh and iterates over every pixel in the texture to discover what the brightest pixel is.  This is done twice, once for the top of the octahedron, and a second time for the bottom of the octahedrone, to ensure the Detector is receiving light from every direction. This method is specifically very similar to the method used by the excellent project, "The Dark Mod". Information on how the detection method was done in that project can be viewed here:
http://forums.thedarkmod.com/topic/18882-how-does-the-light-awareness-system-las-work/


There are several advantages and disadvantages with the way this has been implemented in this UE5 project: 

ADVANTAGES:
* This method factors in bounced lighting. With a raycasting implementation, if line of sight between a light source and the player is broken by some object in the way (eg, a pillar), the player will register as completely invisible. This is obviously not accurate since indirect lighting bounces around the pillar and will still illuminate the player to some extent. Because the Light Detector object samples light data from the Volumetric Lightmaps, this indirect lighting is accounted for. 
* This method factors in percieved luminance. Not all light sources will illuminate objects equally. Some colors, such as red, do not illuminate objects as well as other colors of light. Raycasting methods would not be able to factor this into this calculations, and most raycasting methods also probably wouldnt factor in other things such as the Radius and Intensity of the light entity. 
* This method works for most light sources, including Spot Lights. Moving lights and different light states are also supported (eg if you turn a light in a room off, the light detector will react accordingly).

DISADVANTAGES:
* This method is only as accurate as your Volumetric Lightmap. If your Lightmap resolution is low, you will notice the results may not be as accurate as you'd like in certain locations, such as standing just to the side of a doorway. Increasing the resolution of your lightmaps can help but performance must be considered. 
* This method appears to have problems with Directional Lights used to simulate the sun. Directional Light seems to bleed through walls often. If a player stands next to an exterior wall, he will be lit up by the outside Directional Light, even if the room he is in is pitch black. I was never able to fix this. This method has also not been tested with some more advanced lighting entities such as Skylights. 


# Considerations

* Consider reducing the size of the RenderTextures. The lower the resolution, the less pixels need to be read each tick. This will improve performance. 
* The octahedron mesh needs to be attatched directly to the third person controller, and seperate from the Light Detector object, because it uses the "Owner No See" option to hide the mesh from being displayed by the third person camera. 
* The material you apply to the octahedron does matter. Different materials will produce different luminance values, for example a very dark material would be less luminant than a fully white material, and produce lower "visibility" values. You should probably try to use a material as close to the material of your character as possible.
* This implementation relies on ReadPixels() to function. This method apparently has a very large performance impact, because it calls FlushRenderingCommands(), which pauses execution of the gamethread until the rendering has caught up. On a small example project like this one, it's not noticeable, but on a large and graphically intensive project, it could cause very noticeable hitches. If you want to avoid this, it would be best to look up how you can write your own implementation of ReadPixels() that does not include FlushRenderingCommands(); I haven't looked into it too much but from limited research this appears to be a possible solution. 
* If you are only receiving 0.0 as a visibility value, ensure that you have set both RenderTargets in the Light Detector blueprint. The function will not work correctly unless both a Bottom and Top RenderTarget are supplied.



# LICENSE

Everything created specifically by me (ie excluding all code and assets that come with the Third Person Template for UE5) exists under the Unlicense. Check the LICENSE file for more details.


As much as I'd like to, please do not expect to recieve help from me about how to apply this project to your own needs. I will be providing no help/support for how to use this project beyond this Readme file. 

# Update Oct 6th 2022
* Turned ALightDetector into ULightDetector (an actor component)
* Optimized code previously was getting 60FPS now getting 120FPS
* reduced render textures to 4x4 .. can also use 8x8 if you need more detail.
* Only run once a second as ReadPixels is expensive
* Changed from CaptureScene (forced render) to CaptureSceneDeferred (will be captured next render)
* used ParallelFor to process both textures at the same time
* used ParallelFor to itterate through pixels
* Simpifiy brigtness math
* Optimized settings applied to SceneCaptureComponents2D on character. (we really only want to detect light)
* Hide character mesh so it doesn't obscure the Light Detector Mesh
* Create flat white material for Light Detector Mesh
* Tested on Win, iOS and Android
* Built in delays for optimization and to randomize the game play a bit. Stop the flip flop of hidden / not hidden

Example used in a content heavy scene. The ULightDetector component is attached to my character. You can see the value on the top left side of the screen. It's between 0 and 255
https://www.youtube.com/watch?v=aEDvIdAgXSs

The little character Jumper will turn her tail light on and off depending on how much the directional light is effecting the Light Detector Mesh. In order for her tail light to not effect the Light Detector Mesh, I set the Light Detector Mesh to Light Channel 2.. And the Directional Light (Sun Light) to effect Channel 1 and 2. This allows you to place invisible lights that have no impact on performance to only effect things on channel 2 (The Light Detector Mesh). This way you can illuminate the Light Detector Mesh with out illuminating the character or scene.


