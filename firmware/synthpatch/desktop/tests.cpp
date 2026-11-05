// tests.cpp - automatic checks of the engine. Run it after every change: if it prints
// "all N checks passed" the behaviour is still right. (This is also what a CI pipeline runs.)
//
// No test framework, just a CHECK macro, so it's easy to read.

#include <cmath>
#include <cstdio>

#include "CvOutput.h"
#include "Engine.h"
#include "ModulatorFactory.h"
#include "PitchTracker.h"

using namespace synth;

static int checks = 0, failures = 0;

// A macro is text replacement done before compiling. #cond turns the condition into a string,
// so a failing check prints exactly which line and expression failed.
#define CHECK(cond)                                                    \
  do {                                                                 \
    checks++;                                                          \
    if (!(cond)) {                                                     \
      failures++;                                                      \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
    }                                                                  \
  } while (0)

static bool near(float a, float b, float tol = 0.01f) { return std::fabs(a - b) <= tol; }

static void testWaveforms() {
  Sine s;
  Triangle t;
  Saw w;
  Square q;
  CHECK(near(s.value(0.25f), 1.0f));
  CHECK(near(t.value(0.0f), -1.0f) && near(t.value(0.5f), 1.0f));
  CHECK(near(w.value(0.0f), -1.0f) && near(w.value(0.75f), 0.5f));
  CHECK(q.value(0.1f) == 1.0f && q.value(0.9f) == -1.0f);
  CHECK(makeWaveform("saw")->name() == "saw");
  CHECK(makeWaveform("banana") == nullptr);
}

static void testParamClamp() {
  Param p("rate", "Hz", 0.01f, 50.0f, 1.0f);
  CHECK(p.set(100.0f) && p.get() == 50.0f);
  CHECK(!p.set(50.0f));  // no change -> false
  p.set(-3.0f);
  CHECK(p.get() == 0.01f);
}

static void testLfo() {
  Lfo lfo("l", "sine", 1.0f, 5.0f, 0.0f);
  float v = 0;
  for (int i = 0; i < 250; i++) v = lfo.tick(0.001f);  // quarter cycle
  CHECK(near(v, 5.0f, 0.05f));
  CHECK(lfo.setShape("square") && lfo.shape() == "square");
  CHECK(!lfo.setShape("nope") && lfo.shape() == "square");  // keeps old shape
  CHECK(lfo.setParam("shape", 1) && lfo.shape() == "triangle");  // index from the TFT
  CHECK(lfo.setParam("offset", 2.0f));
  float o = 0;
  CHECK(lfo.getParam("offset", o) && o == 2.0f);
}

static void testEnvelope() {
  Envelope env("e");
  env.setParam("attack", 0.1f);
  env.setParam("decay", 0.1f);
  env.setParam("sustain", 0.5f);
  env.setParam("release", 0.2f);
  env.setParam("peak", 10.0f);
  CHECK(env.tick(0.001f) == 0.0f);  // idle
  env.gate(true);
  float v = 0;
  for (int i = 0; i < 100; i++) v = env.tick(0.001f);
  CHECK(near(v, 10.0f, 0.2f));  // end of attack
  for (int i = 0; i < 200; i++) v = env.tick(0.001f);
  CHECK(env.stage() == Envelope::Stage::Sustain && near(v, 5.0f));
  env.gate(false);
  for (int i = 0; i < 200; i++) v = env.tick(0.001f);
  CHECK(env.stage() == Envelope::Stage::Idle && v == 0.0f);
}

static void testEngineCommands() {
  MemoryOutput out(-5, 5);
  Engine engine;
  engine.add(ModulatorFactory::create("lfo", "lfo1"), out);

  int events = 0, errors = 0;
  float lastValue = 0;
  engine.paramChanged.connect([&](const ParamEvent &e) {  // [&] = capture by reference
    events++;
    lastValue = e.value;
  });
  engine.error.connect([&](const std::string &) { errors++; });

  engine.post(Command::setParam("lfo1", "rate", 1000.0f, "test"));
  CHECK(events == 0);  // nothing happens until the engine task runs
  engine.tick(0.001f);
  CHECK(events == 1 && lastValue == 50.0f);  // clamped value is reported

  engine.post(Command::setParam("lfo2", "rate", 1.0f, "test"));      // unknown id
  engine.post(Command::setParam("lfo1", "volume", 1.0f, "test"));    // unknown param
  engine.post(Command::setShape("lfo1", "hexagon", "test"));         // unknown shape
  engine.tick(0.001f);
  CHECK(errors == 3);

  // Output is clamped by the jack's range even if offset + depth would go higher.
  engine.post(Command::setParam("lfo1", "offset", 5.0f, "test"));
  engine.post(Command::setShape("lfo1", "square", "test"));
  engine.tick(0.001f);
  CHECK(out.last() <= 5.0f);
}

static void testFactory() {
  CHECK(ModulatorFactory::create("lfo", "a")->type() == "lfo");
  CHECK(ModulatorFactory::create("envelope", "b")->type() == "envelope");
  CHECK(ModulatorFactory::create("sequencer", "c") == nullptr);
  CHECK(!ModulatorFactory::create("envelope", "d")->setShape("sine"));  // envelopes have no shape
}

// Pitch tracker, fed with generated frequencies: 20 blocks per second (50 ms each).
static float hzAt(float cents) { return 440.0f * std::pow(2.0f, (cents - 6900.0f) / 1200.0f); }

static void testPitchTracker() {
  CHECK(near(PitchTracker::hzToCents(440.0f), 6900.0f));
  CHECK(PitchTracker::noteName(69) == "A4" && PitchTracker::noteName(60) == "C4");

  PitchTracker pt;
  int events = 0, lastOld = -2, lastNew = -2;
  pt.noteChanged.connect([&](int o, int n) { events++; lastOld = o; lastNew = n; });

  pt.addBlock(0.0f, 0.05f);
  CHECK(pt.state() == PitchTracker::State::NoInput);

  // A4 held for 2 s, 3 cents sharp: one note event (none -> A4), offset +3.
  for (int i = 0; i < 40; i++) pt.addBlock(hzAt(6903.0f), 0.05f);
  CHECK(pt.state() == PitchTracker::State::Stable);
  CHECK(events == 1 && lastOld == -1 && lastNew == 69);
  CHECK(near(pt.centsOff(), 3.0f, 0.1f));

  // Play C5: a jump of 300 cents -> one event A4 -> C5 once it is stable.
  for (int i = 0; i < 40; i++) pt.addBlock(hzAt(7203.0f), 0.05f);
  CHECK(events == 2 && lastOld == 69 && lastNew == 72);

  // Slow drift: +1 cent per second for 60 s. No new note, drift goes up, rate is positive.
  float c = 7203.0f;
  for (int i = 0; i < 1200; i++) {
    c += 0.05f;
    pt.addBlock(hzAt(c), 0.05f);
  }
  CHECK(events == 2);
  CHECK(pt.note() == 72);
  CHECK(pt.drift() > 20.0f && pt.drift() < 63.0f);  // lags behind the raw +63 (30 s average)
  CHECK(pt.driftPerMinute() > 20.0f);

  // Drifting past +50 cents does not invent a note change: offset keeps counting from C5.
  CHECK(pt.centsOff() > 50.0f);
}

int main() {
  testWaveforms();
  testParamClamp();
  testLfo();
  testEnvelope();
  testEngineCommands();
  testFactory();
  testPitchTracker();
  if (failures) {
    std::printf("%d of %d checks FAILED\n", failures, checks);
    return 1;
  }
  std::printf("all %d checks passed\n", checks);
  return 0;
}
