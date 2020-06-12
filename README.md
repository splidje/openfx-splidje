# AJPTechnical OpenFX Effects
Various OpenFX Effects. Only just started this.

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
