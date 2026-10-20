xof 0303txt 0032
// Written by tools/gen_workshop.py for the Pipeline Workshop (room wk-animation). Two frames, no mesh, one AnimationSet that
// moves the Arm frame along +X with position keys at ticks 0, 5 and 10, at 10 ticks per second (0 s, 0.5 s, 1 s), and a
// second animation on the Hand frame with matrix keys at ticks 0 and 10 (a lift of 0.25 along +Y).

AnimTicksPerSecond {
  10;
}

Frame Root {
  FrameTransformMatrix {
    1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0;;
  }
  Frame Arm {
    FrameTransformMatrix {
      1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0;;
    }
    Frame Hand {
      FrameTransformMatrix {
        1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0;;
      }
    }
  }
}

AnimationSet Wave {
  Animation {
    { Arm }
    AnimationKey {
      2;
      3;
      0;3;0.0,0.0,0.0;;,
      5;3;1.0,0.0,0.0;;,
      10;3;2.0,0.0,0.0;;;
    }
  }
  Animation {
    { Hand }
    AnimationKey {
      4;
      2;
      0;16;1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0;;,
      10;16;1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.25,0.0,1.0;;;
    }
  }
}
