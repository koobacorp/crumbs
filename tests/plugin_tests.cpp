#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <chrono>
#include <cstdio>
using namespace cr;

static int passed = 0, failed = 0;
#define CHECK(x) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
#define CHECKM(x, ...) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s | ", __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void addKick(std::vector<float>& L, std::vector<float>& R, int at, double sr, float amp = 0.9f) {
    int n = (int)L.size(); float ph = 0;
    for (int i = 0; i < (int)(0.4 * sr) && at + i < n; ++i) {
        float t = float(i) / float(sr);
        float f = 50.f * std::exp2(18.f * std::exp(-t / 0.008f) / 12.f);
        ph += 2.f * float(M_PI) * f / float(sr);
        float s = amp * std::exp(-t / 0.28f) * std::sin(ph);
        L[(size_t)(at + i)] += s; R[(size_t)(at + i)] += s;
    }
}
static void addHat(std::vector<float>& L, std::vector<float>& R, int at, double sr, float amp = 0.35f) {
    int n = (int)L.size(); uint32_t rng = 0xC0FFEEu + (uint32_t)at;
    float hp = 0, a = 1.f - std::exp(-2.f * float(M_PI) * 7000.f / float(sr));
    for (int i = 0; i < (int)(0.15 * sr) && at + i < n; ++i) {
        float t = float(i) / float(sr);
        rng = rng * 1664525u + 1013904223u;
        float noise = (float((rng >> 9) & 0xFFFF) / 32768.f - 1.f);
        hp += a * (noise - hp);
        float s = amp * (noise - hp) * std::exp(-t / 0.04f);
        L[(size_t)(at + i)] += s; R[(size_t)(at + i)] += s;
    }
}

struct Rig {
    CrumbsProcessor p;
    double sr; int block;
    std::vector<float> L, R, inL, inR;
    Rig(double rate = 44100, int bs = 256) : sr(rate), block(bs) {
        p.setRateAndBufferSizeDetails(sr, block);
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        p.setBusesLayout(layout);
        p.prepareToPlay(sr, block);
    }
    void fillKit(double seconds) {
        int n = (int)(seconds * sr);
        inL.assign((size_t)n, 0); inR.assign((size_t)n, 0);
        int step = (int)std::lround(sr * 60.0 / 120.0);
        for (int i = 0; i < n; i += step) {
            addKick(inL, inR, i, sr, 0.9f);
            addHat(inL, inR, i + step / 2, sr, 0.35f);
        }
    }
    double run(const std::vector<float>& srcL, const std::vector<float>& srcR) {
        int nCh = p.getTotalNumOutputChannels();
        L.clear(); R.clear();
        int n = (int)srcL.size(), pos = 0;
        juce::AudioBuffer<float> buf(std::max(2, nCh), block);
        double t = 0;
        while (pos < n) {
            int m = std::min(block, n - pos);
            buf.setSize(std::max(2, nCh), m, false, false, true);
            for (int i = 0; i < m; ++i) {
                buf.setSample(0, i, srcL[(size_t)(pos + i)]);
                buf.setSample(1, i, srcR[(size_t)(pos + i)]);
            }
            juce::MidiBuffer midi;
            auto t0 = std::chrono::steady_clock::now();
            p.processBlock(buf, midi);
            t += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            L.insert(L.end(), buf.getReadPointer(0), buf.getReadPointer(0) + m);
            R.insert(R.end(), buf.getReadPointer(1), buf.getReadPointer(1) + m);
            pos += m;
        }
        return t;
    }
    double runKit(double seconds) { fillKit(seconds); return run(inL, inR); }
    float peak() const {
        float pk = 0; for (float v : L) pk = std::max(pk, std::abs(v)); for (float v : R) pk = std::max(pk, std::abs(v)); return pk;
    }
    bool finite() const {
        for (float v : L) if (!std::isfinite(v)) return false;
        for (float v : R) if (!std::isfinite(v)) return false;
        return true;
    }
    int identicalToInput() const {
        int s = 0, n = (int)std::min(L.size(), inL.size());
        for (int i = 0; i < n; ++i) if (L[(size_t)i] == inL[(size_t)i] && R[(size_t)i] == inR[(size_t)i]) ++s;
        return s;
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
    std::puts("CRUMBS v2 plugin tests");

    { std::puts("- identity, buses, parameters");
      Rig r;
      CHECK(r.p.getName() == "CRUMBS");
      CHECK(!r.p.acceptsMidi() && !r.p.producesMidi() && !r.p.isMidiEffect());
      CHECK(r.p.getLatencySamples() == 0);
      CHECK((int)r.p.getParameters().size() == kNumParams);
      int automatable = 0, texts = 0;
      for (auto* prm : r.p.getParameters()) { automatable += prm->isAutomatable(); texts += prm->getCurrentValueAsText().isNotEmpty(); }
      CHECK(automatable == kNumParams); CHECK(texts == kNumParams);
      juce::AudioProcessor::BusesLayout lay;
      lay.inputBuses.add(juce::AudioChannelSet::stereo());
      lay.outputBuses.add(juce::AudioChannelSet::stereo());
      CHECK(r.p.checkBusesLayoutSupported(lay));
      juce::AudioProcessor::BusesLayout inst;
      inst.outputBuses.add(juce::AudioChannelSet::stereo()); // no input — instrument layout
      CHECK(!r.p.checkBusesLayoutSupported(inst));
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
      // restore defaults
      r.p.applyBusPreset(0); r.p.applyWholePreset(0);
      r.p.setValue(G_MODE, 0); r.p.setValue(G_AB, 0); r.p.setValue(G_MIX, 1); r.p.setValue(G_OUTPUT, 0);
      r.p.setValue(G_LOOKAHEAD, 0);
      std::printf("  %d parameters, all automatable, stereo insert\n", kNumParams);
    }

    { std::puts("- Clean bit-exact null, 0 latency");
      Rig r;
      CHECK(r.p.getLatencySamples() == 0);
      r.runKit(1.5);
      CHECK(r.finite());
      CHECKM(r.identicalToInput() == (int)r.inL.size(), "clean identical %d/%d peak in %f out %f",
             r.identicalToInput(), (int)r.inL.size(), [&]{ float p=0; for (float v:r.inL) p=std::max(p,std::abs(v)); return p; }(), r.peak());
      CHECK(r.p.getLatencySamples() == 0);
    }

    { std::puts("- A/B dry is a null, Mix 0 with processing respects ceiling or matches dry");
      Rig r;
      r.p.setValue(W_PUNCH, 0.8f);
      r.p.setValue(G_AB, 1);
      r.runKit(1.0);
      CHECK(r.finite() && r.identicalToInput() == (int)r.inL.size());
      r.p.setValue(G_AB, 0);
      r.p.setValue(G_MIX, 0);
      r.runKit(1.0);
      float ceil = dbToGain(r.p.value(G_CEILING));
      CHECK(r.finite() && r.peak() <= std::max(ceil, [&]{ float p=0; for (float v:r.inL) p=std::max(p,std::abs(v)); return p; }()) + 1e-5f);
    }

    { std::puts("- latency report matches impulse");
      for (double sr : {44100.0, 48000.0}) {
        Rig r(sr, 64);
        r.p.setValue(W_PUNCH, 0.4f);
        r.p.setValue(W_CRUNCH, 0.5f);
        r.p.prepareToPlay(sr, 64);
        CHECKM(r.p.getLatencySamples() == 0, "processed path latency %d at %.0f", r.p.getLatencySamples(), sr);
        std::vector<float> x((size_t)sr / 20, 0.f), z = x;
        x[0] = 1; z[0] = 1;
        r.run(x, z);
        int k = 0; float pk = -1;
        for (int i = 0; i < (int)r.L.size(); ++i) if (std::abs(r.L[(size_t)i]) > pk) { pk = std::abs(r.L[(size_t)i]); k = i; }
        CHECKM(k == 0, "impulse peak at %d sr=%.0f", k, sr);
        CHECK(r.peak() <= dbToGain(-0.1f) + 1e-5f);
      }
      Rig r(48000, 64);
      r.p.setValue(G_LOOKAHEAD, 1);
      r.p.prepareToPlay(48000, 64);
      int lat = r.p.getLatencySamples();
      int want = (int)std::lround(0.0015 * 48000);
      CHECKM(lat == want, "reported %d want %d", lat, want);
      std::vector<float> x(4000, 0.f), z = x; x[0] = 1; z[0] = 1;
      r.run(x, z);
      int k = 0; float pk = -1;
      for (int i = 0; i < (int)r.L.size(); ++i) if (std::abs(r.L[(size_t)i]) > pk) { pk = std::abs(r.L[(size_t)i]); k = i; }
      CHECKM(k == lat, "lookahead impulse at %d want %d", k, lat);
      // host bypass must also delay
      r.p.reset();
      std::vector<float> a(4000, 0.f), b = a; a[0] = 0.7f; b[0] = 0.7f;
      // processBlockBypassed
      {
        int n = (int)a.size(), pos = 0;
        juce::AudioBuffer<float> buf(2, r.block);
        r.L.clear(); r.R.clear();
        while (pos < n) {
            int m = std::min(r.block, n - pos);
            buf.setSize(2, m, false, false, true);
            for (int i = 0; i < m; ++i) { buf.setSample(0, i, a[(size_t)(pos+i)]); buf.setSample(1, i, b[(size_t)(pos+i)]); }
            juce::MidiBuffer midi;
            r.p.processBlockBypassed(buf, midi);
            r.L.insert(r.L.end(), buf.getReadPointer(0), buf.getReadPointer(0)+m);
            pos += m;
        }
        int k2 = 0; float p2 = -1;
        for (int i = 0; i < (int)r.L.size(); ++i) if (std::abs(r.L[(size_t)i]) > p2) { p2 = std::abs(r.L[(size_t)i]); k2 = i; }
        CHECKM(k2 == lat, "bypassed lookahead impulse at %d want %d", k2, lat);
      }
    }

    { std::puts("- ceiling on Smashed Room maxed and stress");
      float ceil = dbToGain(-0.1f);
      auto stress = [&](const char* name, auto setup, double sr, int bs, float boost) {
          Rig r(sr, bs);
          setup(r);
          r.fillKit(1.5);
          for (auto& v : r.inL) v *= boost; for (auto& v : r.inR) v *= boost;
          if ((int)r.inL.size() > 300) for (int i = 200; i < 240; ++i) { r.inL[(size_t)i] = boost; r.inR[(size_t)i] = -boost; }
          r.run(r.inL, r.inR);
          CHECKM(r.finite(), "%s not finite", name);
          CHECKM(r.peak() <= ceil + 1e-5f, "%s peak %f > ceil %f", name, r.peak(), ceil);
          std::printf("    %-28s  peak=%.5f\n", name, r.peak());
      };
      stress("Smashed max 44.1", [](Rig& r){ r.p.applyBusPreset(BUS_SMASHED); r.p.setValue(B_PUNCH,1); r.p.setValue(B_CRUNCH,1); r.p.setValue(B_SQUASH,1); r.p.setValue(B_ROOM,1); }, 44100, 512, 1.f);
      stress("Smashed max 48k +6 dB", [](Rig& r){ r.p.applyBusPreset(BUS_SMASHED); r.p.setValue(B_PUNCH,1); r.p.setValue(B_CRUNCH,1); r.p.setValue(B_SQUASH,1); }, 48000, 256, 2.f);
      stress("Whole max + smashed", [](Rig& r){ r.p.applyWholePreset(WP_WIDE); r.p.setValue(W_PUNCH,1); r.p.setValue(W_CRUNCH,1); r.p.setValue(W_GLUE,1); r.p.applyBusPreset(BUS_SMASHED); r.p.setValue(B_PUNCH,1); r.p.setValue(B_CRUNCH,1); r.p.setValue(B_SQUASH,1); }, 48000, 64, 2.f);
      stress("Split max + smashed", [](Rig& r){
          r.p.setValue(G_MODE, 1);
          for (int i = 0; i < kParts; ++i) { r.p.setValue(pp(i,P_LEVEL), 2); r.p.setValue(pp(i,P_PUNCH), 1); r.p.setValue(pp(i,P_CRUNCH), 1); }
          r.p.applyBusPreset(BUS_SMASHED); r.p.setValue(B_PUNCH,1); r.p.setValue(B_CRUNCH,1); r.p.setValue(B_SQUASH,1);
      }, 44100, 1024, 2.f);
    }

    { std::puts("- every control, finite, Clean back to null");
      Rig r; r.fillKit(0.8);
      int ids[] = { W_PUNCH, W_BODY, W_SNAP, W_CRUNCH, W_TONE, W_GLUE, W_ROOM, W_WIDTH,
                    B_PUNCH, B_CRUNCH, B_SQUASH, B_PUMP, B_ROOM, G_MIX, G_OUTPUT, G_CEILING,
                    S_LOW, S_HIGH, W_CRTYPE, B_CRTYPE, G_MODE,
                    pp(0,P_LEVEL), pp(0,P_PUNCH), pp(0,P_BODY), pp(0,P_DECAY), pp(0,P_CRUNCH),
                    pp(0,P_PAN), pp(0,P_WIDTH), pp(0,P_SENS) };
      for (int id : ids) {
          auto* prm = r.p.param(id);
          prm->setValueNotifyingHost(0.7f);
          r.run(r.inL, r.inR);
          CHECKM(r.finite() && r.peak() <= dbToGain(-0.1f) + 1e-4f, "param %d peak %f", id, r.peak());
      }
      r.p.applyBusPreset(0); r.p.applyWholePreset(0);
      r.p.setValue(G_MODE, 0); r.p.setValue(G_MIX, 1); r.p.setValue(G_OUTPUT, 0); r.p.setValue(G_LOOKAHEAD, 0);
      for (int i = 0; i < kParts; ++i) {
          r.p.setValue(pp(i,P_LEVEL), 1); r.p.setValue(pp(i,P_PUNCH), 0); r.p.setValue(pp(i,P_BODY), 0);
          r.p.setValue(pp(i,P_DECAY), 0); r.p.setValue(pp(i,P_CRUNCH), 0); r.p.setValue(pp(i,P_PAN), 0);
          r.p.setValue(pp(i,P_WIDTH), 1); r.p.setValue(pp(i,P_MUTE), 0); r.p.setValue(pp(i,P_LISTEN), 0); r.p.setValue(pp(i,P_SENS), 0.5f);
      }
      r.run(r.inL, r.inR);
      CHECKM(r.identicalToInput() == (int)r.inL.size(), "return to clean identical %d/%d", r.identicalToInput(), (int)r.inL.size());
    }

    { std::puts("- bus presets differ from Clean and stay under the ceiling");
      Rig r; r.fillKit(1.0);
      r.run(r.inL, r.inR);
      auto clean = r.L;
      for (int i = 1; i <= 4; ++i) {
          r.p.applyBusPreset(i);
          r.run(r.inL, r.inR);
          CHECK(r.finite() && r.peak() > 0.02f && r.peak() <= dbToGain(-0.1f) + 1e-5f);
          int same = 0; for (size_t k = 0; k < r.L.size(); ++k) if (r.L[k] == clean[k]) ++same;
          CHECKM(same < (int)r.L.size() / 2, "preset %d too close to clean", i);
      }
      r.p.applyBusPreset(0);
      r.run(r.inL, r.inR);
      CHECK(r.identicalToInput() == (int)r.inL.size());
    }

    { std::puts("- state round-trip");
      Rig r;
      r.p.applyBusPreset(BUS_TAPE);
      r.p.setValue(G_MODE, 1);
      r.p.setValue(W_PUNCH, 0.4f);
      juce::MemoryBlock st; r.p.getStateInformation(st);
      Rig b;
      b.p.setStateInformation(st.getData(), (int)st.getSize());
      CHECK(std::abs(b.p.value(W_PUNCH) - 0.4f) < 1e-4f);
      CHECK((int)std::lround(b.p.value(G_MODE)) == 1);
      CHECK((int)std::lround(b.p.value(B_BUS)) == BUS_TAPE);
    }

    { std::puts("- CPU budget on the plugin path");
      Rig r(48000, 512);
      r.p.setValue(G_MODE, 1);
      r.p.applyBusPreset(BUS_SMASHED);
      r.p.setValue(W_CRUNCH, 0.5f);
      for (int i = 0; i < kParts; ++i) { r.p.setValue(pp(i,P_PUNCH), 0.5f); r.p.setValue(pp(i,P_CRUNCH), 0.4f); }
      r.fillKit(2.0);
      double t = r.run(r.inL, r.inR);
      int blocks = ((int)r.inL.size() + 511) / 512;
      double per = 1000.0 * t / std::max(1, blocks);
      std::printf("    plugin CPU: %.3f ms / 512 block (realtime 10.667 ms), 2s in %.2f ms\n", per, t * 1000);
      CHECKM(per < 1.0, "plugin CPU %.3f ms/block", per);
      CHECK(r.finite() && r.peak() <= dbToGain(-0.1f) + 1e-5f);
    }

    { std::puts("- editor screenshots");
      Rig r;
      std::unique_ptr<juce::AudioProcessorEditor> ed(r.p.createEditor());
      auto* ce = dynamic_cast<CrumbsEditor*>(ed.get());
      CHECK(ce != nullptr);
      ce->setSize(1080, 700);
      juce::Thread::sleep(30);
      savePng(*ce, "v2-whole-clean.png");

      r.p.applyWholePreset(WP_FAT);
      ce->resized(); juce::Thread::sleep(20);
      savePng(*ce, "v2-whole-fatbody.png");

      r.p.applyWholePreset(WP_CLEAN);
      r.p.setValue(G_MODE, 1);
      ce->resized(); juce::Thread::sleep(20);
      savePng(*ce, "v2-split-orbs.png");

      r.p.setSelectedPart(0);
      r.p.setValue(pp(0, P_LISTEN), 1);
      ce->resized(); juce::Thread::sleep(20);
      savePng(*ce, "v2-split-listen-kick.png");

      r.p.setValue(pp(0, P_LISTEN), 0);
      r.p.setValue(G_MODE, 0);
      r.p.applyBusPreset(BUS_SMASHED);
      ce->resized(); juce::Thread::sleep(20);
      savePng(*ce, "v2-bus-smashed.png");
      ed.reset();
    }

    std::printf("PLUGIN: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
