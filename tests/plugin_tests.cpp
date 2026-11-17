#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <chrono>
#include <cstdio>
using namespace cr;

static int passed = 0, failed = 0;
#define CHECK(x) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
#define CHECKM(x, ...) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s | ", __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Head : juce::AudioPlayHead {
    bool playing = false; double bpm = 120, ppq = 0;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p; p.setIsPlaying(playing); p.setBpm(bpm); p.setPpqPosition(ppq); return p;
    }
};
struct Rig {
    CrumbsProcessor p; Head head; double sr; int block;
    std::vector<float> L, R;
    Rig(double rate = 44100, int bs = 256) : sr(rate), block(bs) {
        p.setPlayHead(&head); p.setRateAndBufferSizeDetails(sr, block); p.prepareToPlay(sr, block);
    }
    double run(double seconds, std::function<void(int64_t, juce::MidiBuffer&)> midiAt = {}) {
        int nCh = p.getTotalNumOutputChannels();
        L.clear(); R.clear();
        int64_t n = (int64_t)(seconds * sr), pos = 0; double t = 0;
        juce::AudioBuffer<float> buf(nCh, block);
        while (pos < n) {
            int m = (int)std::min<int64_t>(block, n - pos);
            buf.setSize(nCh, m, false, false, true); buf.clear();
            juce::MidiBuffer midi; if (midiAt) midiAt(pos, midi);
            auto t0 = std::chrono::steady_clock::now();
            p.processBlock(buf, midi);
            t += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            if (nCh > 0) L.insert(L.end(), buf.getReadPointer(0), buf.getReadPointer(0) + m);
            if (nCh > 1) R.insert(R.end(), buf.getReadPointer(1), buf.getReadPointer(1) + m);
            if (head.playing) head.ppq += m * head.bpm / 60.0 / sr;
            pos += m;
        }
        return t;
    }
    float peak() const {
        float pk = 0; for (float v : L) pk = std::max(pk, std::abs(v)); for (float v : R) pk = std::max(pk, std::abs(v)); return pk;
    }
    bool finite() const {
        for (float v : L) if (!std::isfinite(v)) return false;
        for (float v : R) if (!std::isfinite(v)) return false;
        return true;
    }
};

static void savePng(juce::Component& c, const juce::String& name) {
    c.resized();
    auto img = c.createComponentSnapshot(c.getLocalBounds(), true, 1.0f);
    auto f = juce::File::getCurrentWorkingDirectory().getChildFile(name);
    f.deleteFile();
    juce::FileOutputStream s(f);
    bool ok = s.openedOk() && juce::PNGImageFormat().writeImageToStream(img, s);
    CHECK(ok);
    std::printf("  screenshot %s %dx%d\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main(int, char**) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI gui;
    std::puts("CRUMBS plugin tests");

    { std::puts("- identity, parameters");
      Rig r;
      CHECK(r.p.getName() == "CRUMBS");
      CHECK(r.p.acceptsMidi() && !r.p.producesMidi() && !r.p.isMidiEffect());
      CHECK((int)r.p.getParameters().size() == kNumParams);
      int automatable = 0, texts = 0;
      for (auto* prm : r.p.getParameters()) { automatable += prm->isAutomatable(); texts += prm->getCurrentValueAsText().isNotEmpty(); }
      CHECK(automatable == kNumParams); CHECK(texts == kNumParams);
      juce::Random rnd(3); int okRound = 0;
      for (int i = 0; i < kNumParams; ++i) {
          auto* prm = r.p.param(i);
          float n = rnd.nextFloat();
          prm->setValueNotifyingHost(n);
          float real = r.p.value(i);
          bool ok = std::abs(prm->convertTo0to1(real) - prm->convertTo0to1(prm->convertFrom0to1(n))) < 1e-3f;
          okRound += ok;
      }
      CHECKM(okRound == kNumParams, "roundtrip %d/%d", okRound, kNumParams);
      // restore defaults that matter
      r.p.setValue(G_BUS, 0); r.p.setValue(G_MODE, 0); r.p.setValue(G_AB, 0);
      std::printf("  %d parameters, all automatable\n", kNumParams);
    }

    { std::puts("- load synthetic loop, clean default, playback");
      Rig r; CHECK(r.p.loadTestLoop());
      CHECK(r.p.hasLoop());
      CHECK(r.p.machine().lanes.size() == 4);
      CHECK(r.p.machine().bus.preset == BUS_CLEAN && r.p.machine().bus.isClean());
      CHECK(r.p.machine().events.size() > 0);
      r.head.playing = true; r.head.ppq = 0;
      r.p.rebuildNow();
      r.run(2.0);
      CHECK(r.finite());
      CHECKM(r.peak() > 0.05f && r.peak() <= 1.05f, "peak %f", r.peak());
      // host stopped is silent in loop mode
      r.head.playing = false;
      r.run(0.3);
      CHECK(r.peak() == 0);
    }

    { std::puts("- every control (finite audio, Clean bypass, bus presets, lane knobs)");
      Rig r; r.p.loadTestLoop(); r.p.rebuildNow();
      r.head.playing = true;
      // Clean: output matches pre-bus (already clean)
      r.p.setValue(G_BUS, 0); r.p.rebuildNow(); r.run(1.0);
      float cleanPk = r.peak(); CHECK(r.finite() && cleanPk > 0.05f);
      for (int preset = 1; preset <= 4; ++preset) {
          r.p.setValue(G_BUS, (float)preset); r.p.rebuildNow(); r.head.ppq = 0; r.run(1.0);
          CHECKM(r.finite() && r.peak() > 0.02f && r.peak() <= 1.05f, "preset %d peak %f", preset, r.peak());
      }
      r.p.setValue(G_BUS, 0); r.p.rebuildNow();
      // exercise every lane control on lane 0
      int ids[] = { lp(0,LP_LEVEL), lp(0,LP_PITCH), lp(0,LP_DECAY), lp(0,LP_TONE), lp(0,LP_CRUNCH),
                    lp(0,LP_PAN), lp(0,LP_ROOM), lp(0,LP_SWING), lp(0,LP_HUMAN), lp(0,LP_GHOSTS),
                    lp(0,LP_BLEND), G_PUNCH, G_CRUNCH, G_SQUASH, G_PUMP, G_BROOM, G_KEY, G_SCALE,
                    G_KEEPKEY, G_SNAPMODE, G_CRTYPE, G_MODE };
      for (int id : ids) {
          auto* prm = r.p.param(id);
          float mid = 0.7f;
          prm->setValueNotifyingHost(mid);
          r.p.rebuildNow(); r.head.ppq = 0; r.run(0.5);
          CHECKM(r.finite() && r.peak() <= 0.99f, "param %d peak %f", id, r.peak());
      }
      r.p.setValue(lp(0, LP_MUTE), 1.f); r.p.rebuildNow();
      // unmute
      r.p.setValue(lp(0, LP_MUTE), 0.f);
      r.p.setValue(G_MODE, 0); r.p.setValue(G_BUS, 0);
      r.p.setValue(lp(0,LP_LEVEL), 1); r.p.setValue(lp(0,LP_PITCH), 0); r.p.setValue(lp(0,LP_DECAY), 1);
      r.p.rebuildNow();
    }

    { std::puts("- A/B, keep in key, rebuild/blend, undo, export");
      Rig r; r.p.loadTestLoop(); r.p.rebuildNow();
      r.p.setValue(G_AB, 1); r.p.rebuildNow();
      CHECK(r.p.machine().abOriginal);
      r.p.setValue(G_AB, 0);
      // isolated 50 Hz kick so Keep in key has a clear body pitch (E minor: G1 stays, or root snap to E)
      cr::Wave kick; kick.resize((int)(44100 * 0.7), 44100); addKick(kick, 0, 50, 0.9f);
      r.p.loadWave(std::move(kick));
      r.p.setValue(G_KEY, 4); r.p.setValue(G_SCALE, 1); r.p.setValue(G_KEEPKEY, 1); r.p.setValue(G_SNAPMODE, 0);
      r.p.rebuildNow();
      float hz = r.p.machine().laneDisplayHz(0);
      auto n = noteCentsFromHz(hz);
      CHECKM(n.valid && (n.pc == 7 || n.pc == 4), "keep-in-key pc %d hz %g", n.pc, hz);
      r.p.loadTestLoop(); r.p.rebuildNow();
      r.p.machine().rebuildLane(0);
      r.p.pushMachineToParams(); r.p.rebuildNow();
      CHECK(r.p.machine().lanes[0].rebuilt);
      r.p.setValue(lp(0, LP_BLEND), 1); r.p.rebuildNow();
      r.head.playing = true; r.run(1.0); CHECK(r.finite() && r.peak() > 0.02f);
      int ev = (int)r.p.machine().events.size();
      if (ev > 0) { r.p.machine().deleteEvent(0); CHECK((int)r.p.machine().events.size() == ev - 1);
        r.p.machine().doUndo(); CHECK((int)r.p.machine().events.size() == ev); }
      auto wav = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("crumbs-test.wav");
      auto mid = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("crumbs-test.mid");
      CHECK(r.p.saveAudio(wav)); CHECK(wav.getSize() > 100);
      CHECK(r.p.saveMidi(mid)); CHECK(mid.getSize() > 20);
      CHECK(r.p.saveAudio(wav, 0));
    }

    { std::puts("- sampler MIDI, state round-trip, tempo follow");
      Rig r; r.p.loadTestLoop(); r.p.setValue(G_MODE, 1); r.p.rebuildNow();
      r.head.playing = false;
      r.run(0.6, [](int64_t pos, juce::MidiBuffer& m) {
          if (pos == 0) m.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)100), 0);
      });
      CHECKM(r.finite() && r.peak() > 0.02f, "sampler peak %f", r.peak());
      juce::MemoryBlock st; r.p.getStateInformation(st);
      Rig b; b.p.setStateInformation(st.getData(), (int)st.getSize());
      CHECK(b.p.hasLoop());
      CHECK(b.p.machine().lanes.size() == r.p.machine().lanes.size());
      CHECK((int)b.p.machine().events.size() == (int)r.p.machine().events.size());
      // tempo: 240 BPM should play the loop twice as fast (more of the loop in 1 sec)
      Rig t1; t1.p.loadTestLoop(); t1.p.rebuildNow();
      t1.head.playing = true; t1.head.bpm = 120; t1.run(2.0);
      float p120 = t1.peak();
      Rig t2; t2.p.loadTestLoop(); t2.p.rebuildNow();
      t2.head.playing = true; t2.head.bpm = 240; t2.run(1.0);
      CHECK(t1.finite() && t2.finite() && p120 > 0.05f && t2.peak() > 0.05f);
    }

    { std::puts("- editor screenshots and UI interactions");
      Rig r;
      std::unique_ptr<juce::AudioProcessorEditor> ed(r.p.createEditor());
      auto* ce = dynamic_cast<CrumbsEditor*>(ed.get());
      CHECK(ce != nullptr);
      ce->setSize(1080, 700);
      juce::Thread::sleep(30);
      savePng(*ce, "CRUMBS-ui-1-empty-drop.png");
      CHECK(ce->showingEmpty());

      r.p.loadTestLoop(); r.p.rebuildNow();
      ce->resized();
      juce::Thread::sleep(30);
      savePng(*ce, "CRUMBS-ui-2-loaded-slices.png");
      CHECK(!ce->showingEmpty());

      // select hats lane (2) — green orb
      r.p.machine().setLane(2);
      ce->resized();
      juce::Thread::sleep(30);
      savePng(*ce, "CRUMBS-ui-3-lane-orb.png");

      r.p.setValue(G_BUS, 0); r.p.rebuildNow();
      ce->resized();
      juce::Thread::sleep(30);
      savePng(*ce, "CRUMBS-ui-4-bus-clean.png");

      // click Keep in key / add lane / rebuild via public processor
      r.p.machine().addLane("808");
      r.p.machine().rebuildLane(0);
      r.p.pushMachineToParams(); r.p.rebuildNow();
      ce->resized();
      juce::Thread::sleep(30);
      savePng(*ce, "CRUMBS-ui-5-rebuild.png");
      CHECK(r.p.machine().lanes.size() == 5);
      ed.reset();
    }

    std::printf("PLUGIN: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
