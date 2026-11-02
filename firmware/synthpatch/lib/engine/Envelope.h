// Envelope.h - ADSR envelope, started and stopped by a gate.
//
//   level
//   peak |    .
//        |   . .
//        |  .   . . . . . . sustain
//        | .                 .
//      0 |.                   . . .
//         A   D      S        R
//        gate on ......... gate off
//
// C++ lesson: an `enum class` for the stage. It is a type of its own (Stage::Attack), it can't
// be mixed up with ints by accident, and the names don't leak into the surrounding namespace.
#pragma once

#include "Modulator.h"

namespace synth {

class Envelope final : public Modulator {
 public:
  enum class Stage { Idle, Attack, Decay, Sustain, Release };

  explicit Envelope(std::string id);

  float tick(float dt) override;
  std::string type() const override { return "envelope"; }
  void gate(bool on) override;

  Stage stage() const { return stage_; }
  static const char *stageName(Stage s);

 private:
  Stage stage_ = Stage::Idle;
  float level_ = 0.0f;  // 0..1, scaled by peak_ at the output
  Param attack_{"attack", "s", 0.001f, 10.0f, 0.01f};
  Param decay_{"decay", "s", 0.001f, 10.0f, 0.2f};
  Param sustain_{"sustain", "", 0.0f, 1.0f, 0.6f};
  Param release_{"release", "s", 0.001f, 10.0f, 0.5f};
  Param peak_{"peak", "V", 0.0f, 10.0f, 10.0f};
};

}  // namespace synth
