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

## FaceTrack

You need to download https://github.com/italojs/facial-landmarks-recognition/raw/master/shape_predictor_68_face_landmarks.dat to the FaceTrack folder to build (it's ~100MB and it's binary data, so I've decided not to add it to the repo).

## TranslateMap

Plug in a uv map of translations per pixel, and those pixels in the source will be translated by that amount.

e.g. Create a radial, resize it to line-up with the corner of the mouth in a picture of a face, put a Multiply under the radial, plug that into the Translations input of the TranslateMap, plug the face into the Source input, tweak the R and G of the Multiply, and you'll end up sort of pin warping the corner of the mouth.
