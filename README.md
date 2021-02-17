# AJPTechnical OpenFX Effects
Various OpenFX Effects. Only just started this.

## PatchMatch

Attempting to implement the algorithm described here:
https://gfx.cs.princeton.edu/pubs/Barnes_2009_PAR/

The node outputs vectors in RG (ignore B) describing the offset from which to get pixels from source to match target.

This means the node can be plugged into the UV input of an IDistort, with the source plugged into the source input, and the result should look something like target (using IDistort to distort the pixels in source to look like target).

## MeshWarp

Not sure if this is the right name. So far this is the only effect, and has only just been started.
I've got it "working" in Natron - though very buggy, I'm still working out OpenFX in general -
however, you should be able to build it and see it "working".

Right now it has parameters controlling the from and to positions of 1 point in a mesh.

Four other points are hard-coded at the corners of the source image bounds.

Four triangles are hard coded joining the user-controlled mesh vertex with each pair of bounds vertices.

There's then a concept of from and to triangles for each of the four triangles.

Using Barycentric coordinates, each pixel inside a triangle is mapped from the from to the to.

A little bit of interpolation does a weighted mix of a grid of four pixels from the source, for when the from coordinates are whole numbers (i.e. not mapping from exactly 1 pixel).

### Plan

The idea is to be able to add/remove points dynamically.

Use Delaunay Triangulation to automatically generate the triangles for the mesh made by the from points.

I'm getting a creeping feeling this isn't quite what I personally want, and maybe I should instead attempt a spline warp effect.
