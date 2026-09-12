// ==========================================================================
//  tests/gui_test.C -- drive the configuration window without a person
// ==========================================================================
//  Opens the real RUN/ConfigUI/ConfigUI.C against a configuration file and
//  its alignctl.sh, reads two module archives through it, sets knobs, saves,
//  reads the file back and reloads. Every assertion prints PASS or FAIL and
//  the process exits non-zero if any failed.
//
//  Work on a COPY of the repository: the test rewrites the configuration file
//  it is given. Both archives must be in that copy's MODULE/ directory.
//
//     cp -r ALICE-ITS2-ML-Alignment-Manager /tmp/mgr && cd /tmp/mgr
//     ./config/alignctl.sh set MODULE_NAME=<new-archive> TRACK_SCHEMA=auto
//     xvfb-run -a root -l -q 'tests/gui_test.C("config/alignment.conf","config/alignctl.sh","<new-archive>","<old-archive>")'
//
//  <new-archive> is a 2026-generation archive (detector-unit, layer
//  selection, adaptive vertex), <old-archive> a 2024-generation one (none of
//  those). Without xvfb-run an X display is needed; the window is mapped but
//  never waits for input. Private members are opened up for the assertions.
// ==========================================================================
#include <TGFrame.h>
#include <TGTab.h>
#include <TGButton.h>
#include <TGLabel.h>
#include <TGTextEntry.h>
#include <TGNumberEntry.h>
#include <TGComboBox.h>
#include <TGListBox.h>
#include <TGTextView.h>
#include <TGCanvas.h>
#include <TGListTree.h>
#include <TGMsgBox.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TRegexp.h>
#include <TList.h>
#include <TObjArray.h>
#include <TObjString.h>
#include <TString.h>
#include <TApplication.h>
#include <map>
#include <string>
#include <cstdio>
#include <cstring>
#define private public
#define protected public
#include "../RUN/ConfigUI/ConfigUI.C"
#undef private
#undef protected

static int nfail = 0, npass = 0;
static void check(bool ok, const char *what) { printf("  %s  %s\n", ok ? "PASS" : "FAIL", what); if (ok) npass++; else nfail++; }
static TString get(const char *ctl, const char *key) {
   TString v = gSystem->GetFromPipe(TString::Format("%s get %s 2>&1", ctl, key));
   return TString(v.Strip(TString::kTrailing, '\n'));
}
static TString labelText(TGLabel *l) { return TString(l->GetText()->GetString()); }
static void selectModule(AlignConfigUI *ui, const char *name) {
   TGTextLBEntry *e;
   for (Int_t i = 0; (e = (TGTextLBEntry *)ui->fModuleName->GetListBox()->GetEntry(i)); ++i)
      if (TString(e->GetText()->GetString()) == name) { ui->fModuleName->Select(i, kFALSE); ui->OnModuleSelected(); return; }
   printf("  FAIL  archive %s is not in the combo (is it in MODULE/?)\n", name); nfail++;
}
static int knobIndex(const char *key) { for (int i = 0; i < kNKnobs; ++i) if (!strcmp(kKnobs[i].key, key)) return i; return -1; }

void gui_test(const char *conf, const char *ctl, const char *newArchive, const char *oldArchive)
{
   gSystem->Exec(TString::Format("%s set MODULE_NAME=%s TRACK_SCHEMA=auto MODULE_LEARNING_METHOD=keep MODULE_DULEVEL=keep MODULE_LAYERS=keep GEOM_BACKEND=o2 >/dev/null 2>&1", ctl, newArchive));
   AlignConfigUI *ui = new AlignConfigUI(gClient->GetRoot(), conf);
   gSystem->ProcessEvents();
   int iq = knobIndex("MODULE_QUALITY_VERTEXING"), ie = knobIndex("MODULE_ETA_SCALE"), ip = knobIndex("MODULE_PT_MAX");

   printf("T1 load with the 2026-generation archive selected\n");
   check(ui->fCapKnown, "inspect read the archive");
   check(ui->Cap("MP_GENERATION") == "2026", "generation 2026");
   check(labelText(ui->fCap[0]).Contains("generation 2026"), "panel line 0 mentions generation 2026");
   check(labelText(ui->fCap[1]).Contains("kBatch"), "panel lists the implemented methods");
   check(ui->fDULevel->IsEnabled(), "DULEVEL combo enabled");
   check(ui->fGeomBackend->IsEnabled(), "backend combo enabled");
   check(ui->fLayersOn && ui->fLayersKeep->IsOn(), "layers: keep ticked on load");
   check(!ui->fLayer[3]->IsEnabled(), "layer boxes disabled while keep");
   check(ui->SelectedName(ui->fMethod) == "keep", "method combo shows keep");
   check(ui->SelectedName(ui->fDULevel) == "keep", "DULEVEL combo shows keep");
   check(ui->fTrackSchema->GetSelected() == 0, "schema combo shows auto");
   check(ui->fKnobOn[iq] && ui->fKnob[iq]->IsEnabled(), "vertex knob enabled");
   check(labelText(ui->fKnobArchive[ie]).BeginsWith("archive: ") && !labelText(ui->fKnobArchive[ie]).Contains("not"), "archive value shown beside eta scale");
   check(ui->fVertexDerivOn, "VERTEX_DERIVATIVES enabled");

   printf("T2 save with knobs set, then read back and reload\n");
   ui->SelectByName(ui->fMethod, "kSteepestDescent");
   ui->SelectByName(ui->fDULevel, "4(module)");
   ui->fLayersKeep->SetState(kButtonUp, kFALSE); ui->OnLayersKeep();
   check(ui->fLayer[5]->IsEnabled(), "layer boxes enabled after unticking keep");
   ui->fLayer[5]->SetState(kButtonDown, kFALSE); ui->fLayer[6]->SetState(kButtonDown, kFALSE); ui->OnLayerToggle();
   check(ui->LayerList() == "5,6", "LayerList reads 5,6");
   ui->fKnob[ie]->SetText("1.0e+3");
   ui->fKnob[ip]->SetText("  ");
   ui->fTrackSchema->Select(2026, kFALSE);
   ui->fVertexDeriv->Select(2, kFALSE);
   ui->OnSave();
   check(get(ctl, "MODULE_LEARNING_METHOD") == "kSteepestDescent", "saved MODULE_LEARNING_METHOD");
   check(get(ctl, "MODULE_DULEVEL") == "4", "saved MODULE_DULEVEL=4");
   check(get(ctl, "MODULE_LAYERS") == "5,6", "saved MODULE_LAYERS=5,6");
   check(get(ctl, "MODULE_ETA_SCALE") == "1.0e+3", "saved MODULE_ETA_SCALE=1.0e+3");
   check(get(ctl, "MODULE_VERTEX_DERIVATIVES") == "TRUE", "saved MODULE_VERTEX_DERIVATIVES=TRUE");
   check(get(ctl, "TRACK_SCHEMA") == "2026", "saved TRACK_SCHEMA=2026");
   check(get(ctl, "MODULE_PT_MAX") == "keep", "blank entry saved as keep");
   ui->OnReload();
   check(ui->SelectedName(ui->fMethod) == "kSteepestDescent", "reload: method combo");
   check(ui->SelectedName(ui->fDULevel) == "4(module)", "reload: DULEVEL combo");
   check(!ui->fLayersKeep->IsOn() && ui->fLayer[5]->IsOn() && ui->fLayer[6]->IsOn() && !ui->fLayer[4]->IsOn(), "reload: layer ticks survive (5,6)");
   check(TString(ui->fKnob[ie]->GetText()) == "1.0e+3", "reload: eta scale entry");
   check(ui->fTrackSchema->GetSelected() == 2026, "reload: schema 2026");
   ui->OnSave();
   check(get(ctl, "MODULE_LAYERS") == "5,6", "second save keeps MODULE_LAYERS=5,6");

   printf("T3 switch to the 2024-generation archive\n");
   selectModule(ui, oldArchive);
   check(ui->fCapKnown && ui->Cap("MP_GENERATION") == "2024", "inspect read the 2024 archive");
   check(!ui->fDULevel->IsEnabled() && ui->SelectedName(ui->fDULevel) == "keep", "DULEVEL disabled and parked at keep");
   check(!ui->fGeomBackend->IsEnabled() && ui->SelectedName(ui->fGeomBackend) == "o2", "backend disabled and parked at o2");
   check(!ui->fLayersOn && ui->fLayersKeep->IsDisabledAndSelected() && !ui->fLayer[5]->IsEnabled(), "layers parked at keep");
   check(!ui->fKnobOn[iq] && TString(ui->fKnob[iq]->GetText()).IsNull(), "vertex knob disabled and cleared");
   check(!ui->fVertexDerivOn, "VERTEX_DERIVATIVES disabled");
   check(labelText(ui->fKnobArchive[iq]) == "archive: not in this module", "vertex knob archive label");
   check(labelText(ui->fLayerInfo).Contains("all seven layers"), "layer info says all seven layers");
   ui->fKnob[ie]->SetText("");
   ui->SelectByName(ui->fMethod, "keep");
   ui->fTrackSchema->Select(0, kFALSE);
   ui->OnSave();
   check(get(ctl, "MODULE_NAME") == oldArchive, "saved MODULE_NAME");
   check(get(ctl, "MODULE_DULEVEL") == "keep" && get(ctl, "MODULE_LAYERS") == "keep", "disabled knobs saved as keep");
   check(get(ctl, "MODULE_QUALITY_VERTEXING") == "keep" && get(ctl, "MODULE_VERTEX_DERIVATIVES") == "keep", "vertex knobs saved as keep");
   check(get(ctl, "GEOM_BACKEND") == "o2" && get(ctl, "TRACK_SCHEMA") == "auto", "backend o2, schema auto");
   check(gSystem->GetFromPipe(TString::Format("%s validate 2>&1", ctl)).Contains("consistent"), "saved configuration validates");

   printf("T4 an archive that is not there\n");
   gSystem->Exec(TString::Format("%s set MODULE_NAME=nowhere >/dev/null 2>&1", ctl));
   ui->OnReload();
   check(!ui->fCapKnown, "capabilities unknown");
   check(labelText(ui->fCap[0]).Contains("not in MODULE/"), "panel says the archive is missing");
   check(ui->fDULevel->IsEnabled() && ui->fGeomBackend->IsEnabled(), "every combo left enabled");
   selectModule(ui, newArchive);
   check(ui->fCapKnown && ui->Cap("MP_GENERATION") == "2026", "picking the 2026 archive re-inspects");
   check(ui->fLayersOn && ui->fLayersKeep->IsEnabled() && ui->fLayersKeep->IsOn(), "keep box re-enabled with its tick restored");
   ui->OnSave();
   check(get(ctl, "MODULE_NAME") == newArchive && get(ctl, "MODULE_LAYERS") == "keep", "saved after the round trip");

   printf("T5 the action buttons\n");
   int before = ui->fLog->ReturnLineCount();
   ui->OnValidate(); ui->OnDoctor(); ui->OnGenerate();
   check(ui->fLog->ReturnLineCount() > before + 10, "validate/doctor/generate output reached the log pane");

   printf("\n%d passed, %d failed\n", npass, nfail);
   ui->UnmapWindow();
   gSystem->Exit(nfail ? 1 : 0);
}
