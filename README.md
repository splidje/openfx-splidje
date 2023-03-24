# Splidje OpenFX Effects
Various OpenFX Effects. Currently all still work-in-progress

## EstimateGrade

You plug in two images which spatially line-up - source and target. You know that source was graded in some way to generate target.
You choose whether you think the curve used was a gamma curve with black and white point, or an S-curve with centre point, slope and gamma.
EstimateGrade does some non-linear curve fitting to work out what the grade may have been, and then processes that grade on source to demonstrate its estimate.

## splidjeCornerPin

Well, perhaps not _quite_ what CornerPin should do. Uses an internal QuadrangleDistort library. It distorts a quadrangle, what can I say!

## PatchMatch

Attempting to implement the algorithm described here:
https://gfx.cs.princeton.edu/pubs/Barnes_2009_PAR/

The node outputs vectors in RG (ignore B) describing the offset from which to get pixels from source to match target.

This means the node can be plugged into the UV input of an IDistort (or this repo's OffsetMap), with the source plugged into the source input, and the result should look something like target (using IDistort to distort the pixels in source to look like target).

## OffsetMap

Perhaps a slightly faster, less fancy version of IDistort. Plug in a source and an image with canonical pixel offsets. The result is pixel values drawn from the source from that position plus the offset.

## TranslateMap

Plug in a uv map of translations per pixel, and those pixels in the source will be translated by that amount.

e.g. Create a radial, resize it to line-up with the corner of the mouth in a picture of a face, put a Multiply under the radial, plug that into the Translations input of the TranslateMap, plug the face into the Source input, tweak the R and G of the Multiply, and you'll end up sort of pin warping the corner of the mouth.

## FaceTrack

You need to download https://github.com/italojs/facial-landmarks-recognition/raw/master/shape_predictor_68_face_landmarks.dat to the FaceTrack folder to build (it's ~100MB and it's binary data, so I've decided not to add it to the repo).

Has a 2d param per face landmark. Clicking the Track button sets the values of those params using dlib. The overlay draws the face it's detected.

## FaceTranslationMap

Plug in Source and Target, it can track a face in both. It can then calculate the movement of the source face relative to a reference frame and use that to generate either a UV Map (to be used by OffsetMap/IDistort) or a UV translation map (to be used by TranslateMap) for moving the target face with the same movements.
