// ==========================================================================
//  ConfigUI.C -- one window for every alignment setting
// ==========================================================================
//  Run it from the repository root, with the O2 environment loaded:
//
//     eval `alienv load -w $O2_DIR/sw O2/latest`
//     ./config/alignctl.sh ui
//
//  or directly:
//
//     root -l 'RUN/ConfigUI/ConfigUI.C("config/alignment.conf")'
//
//  The window never writes the configuration file itself. Save shells out to
//  config/alignctl.sh, so the file format and every validation rule stay in
//  one place and the GUI and the command line cannot drift apart. What the
//  selected module archive can do is read the same way, through
//  `alignctl.sh inspect`, so the window and the driver gate on one report.
//
//  Tested against ROOT 6 (the version O2 provides). CentOS 7 needs no extra
//  packages -- everything here is ROOT's own GUI toolkit.
// ==========================================================================

#include "DirBrowser.h"

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

// The numeric module knobs: configuration key, label, and the inspect field
// holding the archive's own value. An empty entry means keep.
struct AlignKnob { const char *key; const char *label; const char *mpkey; };
static const AlignKnob kKnobs[] = {
   { "MODULE_DET_MAG",             "DET_MAG (T)",              "MP_MOD_DET_MAG" },
   { "MODULE_NTRACKMAX",           "nTrackMax",                "MP_MOD_NTRACKMAX" },
   { "MODULE_PT_MIN",              "Update_pTmin (GeV/c)",     "MP_MOD_PT_MIN" },
   { "MODULE_PT_MAX",              "Update_pTmax (GeV/c)",     "MP_MOD_PT_MAX" },
   { "MODULE_CHI_IB",              "RANGE_CHI_IB",             "MP_MOD_CHI_IB" },
   { "MODULE_CHI_OB",              "RANGE_CHI_OB",             "MP_MOD_CHI_OB" },
   { "MODULE_CHI_IB_TRAIN",        "RANGE_CHI_IB_TRAINING",    "MP_MOD_CHI_IB_TRAIN" },
   { "MODULE_CHI_OB_TRAIN",        "RANGE_CHI_OB_TRAINING",    "MP_MOD_CHI_OB_TRAIN" },
   { "MODULE_TRACK_REJECT",        "TrackRejection",           "MP_MOD_TRACK_REJECT" },
   { "MODULE_IP_RANGE_R",          "RANGE_IMPACTPARAMS_R",     "MP_MOD_IP_RANGE_R" },
   { "MODULE_IP_RANGE_Z",          "RANGE_IMPACTPARAMS_Z",     "MP_MOD_IP_RANGE_Z" },
   { "MODULE_MIN_CLUSTER",         "Min_Cluster_by_Sensor",    "MP_MOD_MIN_CLUSTER" },
   { "MODULE_ETA_CONSTANT",        "eta: UpdateConstant",      "MP_MOD_ETA_CONSTANT" },
   { "MODULE_ETA_SCALE",           "eta: UpdateScale",         "MP_MOD_ETA_SCALE" },
   { "MODULE_ETA_DETRES",          "eta: DETRES (um)",         "MP_MOD_ETA_DETRES" },
   { "MODULE_VALID_WINDOW",        "ValidWindow",              "MP_MOD_VALID_WINDOW" },
   { "MODULE_QUALITY_VERTEXING",   "QUALITY_VERTEXING",        "MP_MOD_QUALITY_VERTEXING" },
   { "MODULE_QUALITY_TRACKVERTEX", "QUALITY_TRACKVERTEX",      "MP_MOD_QUALITY_TRACKVERTEX" },
   { "MODULE_MAX_BAD_TRACKS",      "max bad prongs (vertex)",  "MP_MOD_MAX_BAD_TRACKS" },
};
static const Int_t kNKnobs = sizeof(kKnobs) / sizeof(kKnobs[0]);

static const char *kMethods =
   "keep kStochastic kBatch kBatchDetectorUnitUser kSteepestDescent "
   "kRibierePolak kFletcherReeves kBFGS kOffsetTuneByMean";

class AlignConfigUI : public TGMainFrame {
private:
   TString fConf;        // configuration file
   TString fCtl;         // config/alignctl.sh
   TString fRoot;        // repository root

   TGTextEntry   *fDataDir;
   TGTextEntry   *fPattern;
   TGListTree    *fFileTree;
   TGCanvas      *fFileCanvas;
   TGCompositeFrame *fFileCountRow;   // holds fFileCount; re-laid out on rescan
   TGNumberEntry *fFilesPerBatch;
   TGNumberEntry *fMergeMax;
   TGLabel       *fFileCount;

   TGCompositeFrame *fModuleTab;
   TGComboBox    *fModuleName;
   TGComboBox    *fTrackSchema;
   TGNumberEntry *fEvents;
   TGNumberEntry *fEpochs;
   TGNumberEntry *fJParallel;
   TGNumberEntry *fCores;
   TGComboBox    *fGeomBackend;
   TGComboBox    *fMethod;
   TGComboBox    *fDULevel;
   TGCheckButton *fLayersKeep;
   TGCheckButton *fLayer[7];
   TGLabel       *fLayerInfo;
   TGGroupFrame  *fCapFrame;          // the capability panel; re-laid out on inspect
   TGLabel       *fCap[6];

   TGCompositeFrame *fTuningTab;
   TGTextEntry   *fKnob[kNKnobs];
   TGLabel       *fKnobArchive[kNKnobs];
   TGComboBox    *fVertexDeriv;
   TGLabel       *fVertexDerivArchive;

   TGNumberEntry *fBaseStep;
   TGNumberEntry *fStepsPerBatch;
   TGNumberEntry *fNBatches;
   TGNumberEntry *fNWorkers;
   TGNumberEntry *fBatchId;
   TGLabel       *fSummary;
   TGCompositeFrame *fSummaryHolder;  // holds fSummary; re-laid out on change

   TGTextEntry   *fO2Dir;
   TGTextEntry   *fMasterDir;
   TGNumberEntry *fStagger;
   TGTextEntry   *fRunTag;

   TGTextView    *fLog;

   // What `alignctl.sh inspect` said about the selected archive. Empty until
   // an archive has been read; a failed read leaves every widget enabled.
   std::map<std::string, std::string> fCapMap;
   Bool_t fCapKnown;
   // Which knobs are enabled, kept here rather than asked of the widgets so
   // Save does not depend on how each widget class reports its state.
   Bool_t fKnobOn[kNKnobs];
   Bool_t fVertexDerivOn;
   Bool_t fLayersOn;

   TString Run(const char *args);
   TString Get(const char *key);
   static TString Quote(const TString &s);
   void Log(const TString &text);
   void LogCommand(const TString &title, const TString &output);

   TGNumberEntry *MakeNumber(TGCompositeFrame *p, Long_t min, Long_t max);
   TGTextEntry   *MakeRow(TGCompositeFrame *p, const char *label, const char *slot);
   TGComboBox    *MakeCombo(TGCompositeFrame *p, const char *label, const char *items,
                            Int_t width, const char *slot);
   static void    SelectByName(TGComboBox *c, const TString &name);
   static TString SelectedName(TGComboBox *c);
   static TString ValueOf(TGComboBox *c);   // the first word of the selected entry
   static void    Enable(TGCheckButton *b, Bool_t on);

   TString Cap(const char *key) const;
   Bool_t  CapIs(const char *key, const char *value) const;
   TString SelectedArchive() const;
   TString LayerList() const;
   void    SetLayerChecks(const TString &list);
   void    ApplyCapabilities();

   void BuildData(TGCompositeFrame *tab);
   void BuildModule(TGCompositeFrame *tab);
   void BuildTuning(TGCompositeFrame *tab);
   void BuildSchedule(TGCompositeFrame *tab);
   void BuildEnvironment(TGCompositeFrame *tab);

public:
   AlignConfigUI(const TGWindow *p, const char *conf);
   virtual ~AlignConfigUI() { Cleanup(); }

   void LoadAll();
   void Rescan();
   void Inspect();
   void UpdateSummary();
   void UpdateLayerInfo();

   void OnBrowseData();
   void OnBrowseO2();
   void OnBrowseMaster();
   void OnModuleSelected();
   void OnLayersKeep();
   void OnLayerToggle();
   void OnRescan();
   void OnReload();
   void OnValidate();
   void OnDoctor();
   void OnGenerate();
   void OnSave();
   void OnQuit();

   ClassDef(AlignConfigUI, 0)
};

// -------------------------------------------------------------- helpers ---

TString AlignConfigUI::Quote(const TString &s)
{
   // POSIX single-quote escaping: close the quote, add an escaped quote,
   // reopen. There is no failure mode, so no caller has to test for one.
   TString out = "'";
   for (Ssiz_t i = 0; i < s.Length(); ++i) {
      if (s[i] == '\'') out += "'\\''";
      else                out += s[i];
   }
   out += "'";
   return out;
}

TString AlignConfigUI::Run(const char *args)
{
   TString cmd = Quote(fCtl) + " " + args + " 2>&1";
   return gSystem->GetFromPipe(cmd);
}

TString AlignConfigUI::Get(const char *key)
{
   TString v = Run(TString::Format("get %s", key));
   v = v.Strip(TString::kTrailing, '\n');
   return v;
}

void AlignConfigUI::Log(const TString &text)
{
   TObjArray *lines = text.Tokenize("\n");
   for (Int_t i = 0; i < lines->GetEntries(); ++i)
      fLog->AddLine(((TObjString *)lines->At(i))->GetString().Data());
   delete lines;
   fLog->ShowBottom();
}

void AlignConfigUI::LogCommand(const TString &title, const TString &output)
{
   fLog->AddLine("");
   fLog->AddLine(TString::Format("--- %s ---", title.Data()).Data());
   Log(output);
}

TGNumberEntry *AlignConfigUI::MakeNumber(TGCompositeFrame *p, Long_t min, Long_t max)
{
   TGNumberEntry *n = new TGNumberEntry(p, 0, 9, -1,
                                        TGNumberFormat::kNESInteger,
                                        TGNumberFormat::kNEANonNegative,
                                        TGNumberFormat::kNELLimitMinMax,
                                        (Double_t)min, (Double_t)max);
   return n;
}

// A labelled text entry with an optional Browse button wired to `slot`.
TGTextEntry *AlignConfigUI::MakeRow(TGCompositeFrame *p, const char *label, const char *slot)
{
   TGHorizontalFrame *row = new TGHorizontalFrame(p);
   TGLabel *l = new TGLabel(row, label);
   l->SetWidth(150);
   row->AddFrame(l, new TGLayoutHints(kLHintsCenterY, 4, 6, 3, 3));

   TGTextEntry *entry = new TGTextEntry(row);
   row->AddFrame(entry, new TGLayoutHints(kLHintsExpandX | kLHintsCenterY, 0, 4, 3, 3));

   if (slot && slot[0]) {
      TGTextButton *browse = new TGTextButton(row, " Browse... ");
      browse->Connect("Clicked()", "AlignConfigUI", this, slot);
      row->AddFrame(browse, new TGLayoutHints(kLHintsCenterY, 0, 4, 3, 3));
   }
   p->AddFrame(row, new TGLayoutHints(kLHintsExpandX, 2, 2, 1, 1));
   return entry;
}

// A labelled combo box whose entries are the space-separated `items`, with
// ids 0, 1, 2, ... so SelectByName can walk them.
TGComboBox *AlignConfigUI::MakeCombo(TGCompositeFrame *p, const char *label, const char *items,
                                     Int_t width, const char *slot)
{
   TGHorizontalFrame *row = new TGHorizontalFrame(p);
   TGLabel *l = new TGLabel(row, label);
   l->SetWidth(150);
   row->AddFrame(l, new TGLayoutHints(kLHintsCenterY, 4, 6, 3, 3));

   TGComboBox *c = new TGComboBox(row);
   TObjArray *parts = TString(items).Tokenize(" ");
   for (Int_t i = 0; i < parts->GetEntries(); ++i)
      c->AddEntry(((TObjString *)parts->At(i))->GetString().Data(), i);
   delete parts;
   c->Resize(width, 22);
   if (slot && slot[0]) c->Connect("Selected(Int_t)", "AlignConfigUI", this, slot);
   row->AddFrame(c, new TGLayoutHints(kLHintsCenterY, 0, 4, 3, 3));
   p->AddFrame(row, new TGLayoutHints(kLHintsExpandX, 2, 2, 1, 1));
   return c;
}

// For combos built by MakeCombo only: the ids are contiguous from 0.
void AlignConfigUI::SelectByName(TGComboBox *c, const TString &name)
{
   TGTextLBEntry *e;
   for (Int_t i = 0; (e = (TGTextLBEntry *)c->GetListBox()->GetEntry(i)); ++i) {
      if (name == e->GetText()->GetString()) { c->Select(i, kFALSE); return; }
   }
}

TString AlignConfigUI::SelectedName(TGComboBox *c)
{
   TGTextLBEntry *e = (TGTextLBEntry *)c->GetSelectedEntry();
   return e ? TString(e->GetText()->GetString()) : TString("");
}

// Entries like "4  module" carry the value first and a description after.
TString AlignConfigUI::ValueOf(TGComboBox *c)
{
   TString v = SelectedName(c);
   Ssiz_t sp = v.Index(" ");
   if (sp > 0) v.Remove(sp);
   return v;
}

// TGButton::SetEnabled(kTRUE) is SetState(kButtonUp), which on a ticked box
// clears the tick. Only change the enabled state when it actually differs,
// and always enable before ticking.
void AlignConfigUI::Enable(TGCheckButton *b, Bool_t on)
{
   if (b->IsEnabled() != on) b->SetEnabled(on);
}

TString AlignConfigUI::Cap(const char *key) const
{
   std::map<std::string, std::string>::const_iterator it = fCapMap.find(key);
   return it == fCapMap.end() ? TString("") : TString(it->second.c_str());
}

Bool_t AlignConfigUI::CapIs(const char *key, const char *value) const
{
   return Cap(key) == value;
}

TString AlignConfigUI::SelectedArchive() const
{
   TGTextLBEntry *sel = (TGTextLBEntry *)fModuleName->GetSelectedEntry();
   if (!sel) return TString("");
   TString name = sel->GetText()->GetString();
   name.ReplaceAll("   (archive not found)", "");
   return name;
}

// ------------------------------------------------------------ construction ---

AlignConfigUI::AlignConfigUI(const TGWindow *p, const char *conf)
   : TGMainFrame(p, 940, 820), fCapKnown(kFALSE), fVertexDerivOn(kTRUE), fLayersOn(kTRUE)
{
   for (Int_t i = 0; i < kNKnobs; ++i) fKnobOn[i] = kTRUE;
   SetCleanup(kDeepCleanup);
   SetWindowName("ITS2 Alignment Configuration");

   fConf = gSystem->ExpandPathName(conf);
   if (!fConf.BeginsWith("/"))
      fConf = TString::Format("%s/%s", gSystem->WorkingDirectory(), fConf.Data());
   TString confDir = gSystem->DirName(fConf);
   fCtl  = confDir + "/alignctl.sh";
   fRoot = gSystem->DirName(confDir);

   TGTab *tabs = new TGTab(this, 920, 580);
   BuildData(tabs->AddTab("Data"));
   BuildModule(tabs->AddTab("Module"));
   BuildTuning(tabs->AddTab("Module tuning"));
   BuildSchedule(tabs->AddTab("Schedule"));
   BuildEnvironment(tabs->AddTab("Environment"));
   AddFrame(tabs, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 4, 4, 4, 2));

   TGHorizontalFrame *bar = new TGHorizontalFrame(this);
   struct { const char *text; const char *slot; } acts[] = {
      { " Reload ",          "OnReload()" },
      { " Validate ",        "OnValidate()" },
      { " Check machine ",   "OnDoctor()" },
      { " Generate ",        "OnGenerate()" },
      { " Save + Generate ", "OnSave()" },
      { 0, 0 }
   };
   for (Int_t i = 0; acts[i].text; ++i) {
      TGTextButton *b = new TGTextButton(bar, acts[i].text);
      b->Connect("Clicked()", "AlignConfigUI", this, acts[i].slot);
      bar->AddFrame(b, new TGLayoutHints(kLHintsLeft, 4, 0, 4, 4));
   }
   TGTextButton *quit = new TGTextButton(bar, " Close ");
   quit->Connect("Clicked()", "AlignConfigUI", this, "OnQuit()");
   bar->AddFrame(quit, new TGLayoutHints(kLHintsRight, 4, 4, 4, 4));
   AddFrame(bar, new TGLayoutHints(kLHintsExpandX, 2, 2, 0, 0));

   fLog = new TGTextView(this, 920, 150);
   AddFrame(fLog, new TGLayoutHints(kLHintsExpandX, 4, 4, 2, 4));

   MapSubwindows();
   Resize(GetDefaultSize());
   MapWindow();

   if (gSystem->AccessPathName(fCtl, kExecutePermission)) {
      Log(TString::Format("cannot execute %s -- check it exists and is +x", fCtl.Data()));
      return;
   }
   LoadAll();
}

void AlignConfigUI::BuildData(TGCompositeFrame *tab)
{
   fDataDir = MakeRow(tab, "Input directory", "OnBrowseData()");
   fPattern = MakeRow(tab, "File pattern", 0);

   TGHorizontalFrame *row = new TGHorizontalFrame(tab);
   TGTextButton *rescan = new TGTextButton(row, " List files ");
   rescan->Connect("Clicked()", "AlignConfigUI", this, "OnRescan()");
   row->AddFrame(rescan, new TGLayoutHints(kLHintsLeft, 158, 8, 4, 4));
   fFileCount = new TGLabel(row, "no directory listed yet");
   row->AddFrame(fFileCount, new TGLayoutHints(kLHintsCenterY, 0, 4, 4, 4));
   fFileCountRow = row;
   tab->AddFrame(row, new TGLayoutHints(kLHintsExpandX, 2, 2, 1, 1));

   fFileCanvas = new TGCanvas(tab, 880, 220);
   fFileTree = new TGListTree(fFileCanvas, kHorizontalFrame);
   fFileCanvas->SetContainer(fFileTree);
   tab->AddFrame(fFileCanvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 6, 6, 2, 2));

   TGHorizontalFrame *nums = new TGHorizontalFrame(tab);
   TGLabel *l1 = new TGLabel(nums, "Files per batch");
   l1->SetWidth(150);
   nums->AddFrame(l1, new TGLayoutHints(kLHintsCenterY, 4, 6, 4, 4));
   fFilesPerBatch = MakeNumber(nums, 1, 1000);
   nums->AddFrame(fFilesPerBatch, new TGLayoutHints(kLHintsCenterY, 0, 20, 4, 4));
   nums->AddFrame(new TGLabel(nums, "Merge cap"), new TGLayoutHints(kLHintsCenterY, 0, 6, 4, 4));
   fMergeMax = MakeNumber(nums, 1, 1000);
   nums->AddFrame(fMergeMax, new TGLayoutHints(kLHintsCenterY, 0, 4, 4, 4));
   tab->AddFrame(nums, new TGLayoutHints(kLHintsExpandX, 2, 2, 1, 1));
}

void AlignConfigUI::BuildModule(TGCompositeFrame *tab)
{
   fModuleTab = tab;

   TGHorizontalFrame *row = new TGHorizontalFrame(tab);
   TGLabel *l = new TGLabel(row, "Module archive");
   l->SetWidth(150);
   row->AddFrame(l, new TGLayoutHints(kLHintsCenterY, 4, 6, 4, 4));
   fModuleName = new TGComboBox(row);
   fModuleName->Resize(560, 22);
   fModuleName->Connect("Selected(Int_t)", "AlignConfigUI", this, "OnModuleSelected()");
   row->AddFrame(fModuleName, new TGLayoutHints(kLHintsExpandX | kLHintsCenterY, 0, 4, 4, 4));
   tab->AddFrame(row, new TGLayoutHints(kLHintsExpandX, 2, 2, 4, 2));

   // What the selected archive has and holds, from `alignctl.sh inspect`.
   fCapFrame = new TGGroupFrame(tab, "Module capabilities (read from the archive)");
   for (Int_t i = 0; i < 6; ++i) {
      fCap[i] = new TGLabel(fCapFrame, "................................................................................................");
      fCapFrame->AddFrame(fCap[i], new TGLayoutHints(kLHintsLeft, 4, 4, 1, 1));
   }
   tab->AddFrame(fCapFrame, new TGLayoutHints(kLHintsExpandX, 158, 6, 2, 6));

   // Track schema ids are the years, plus 0 for auto; OnSave maps them back
   // to the strings the file uses.
   TGHorizontalFrame *schemaRow = new TGHorizontalFrame(tab);
   TGLabel *sl = new TGLabel(schemaRow, "Track schema");
   sl->SetWidth(150);
   schemaRow->AddFrame(sl, new TGLayoutHints(kLHintsCenterY, 4, 6, 3, 3));
   fTrackSchema = new TGComboBox(schemaRow);
   fTrackSchema->AddEntry("auto   (read from the archive)", 0);
   fTrackSchema->AddEntry("2024   (no per-track charge)", 2024);
   fTrackSchema->AddEntry("2025   (per-track charge)", 2025);
   fTrackSchema->AddEntry("2026   (per-track charge, same tree as 2025)", 2026);
   fTrackSchema->Resize(320, 22);
   schemaRow->AddFrame(fTrackSchema, new TGLayoutHints(kLHintsCenterY, 0, 4, 3, 3));
   tab->AddFrame(schemaRow, new TGLayoutHints(kLHintsExpandX, 2, 2, 0, 0));

   fGeomBackend = MakeCombo(tab, "Geometry backend", "o2 cache", 120, 0);
   fMethod      = MakeCombo(tab, "Learning method", kMethods, 240, 0);
   fDULevel     = MakeCombo(tab, "DULEVEL (unit)",
                            "keep -1(whole-detector) 0(half-barrel) 1(layer) 2(half-stave) 3(stave) 4(module) 5(chip)",
                            240, 0);

   // Layers the batch update may move: a keep box for the archive's own
   // selection, else one box per layer.
   TGHorizontalFrame *lrow = new TGHorizontalFrame(tab);
   TGLabel *ll = new TGLabel(lrow, "Layers (batch path)");
   ll->SetWidth(150);
   lrow->AddFrame(ll, new TGLayoutHints(kLHintsCenterY, 4, 6, 3, 3));
   fLayersKeep = new TGCheckButton(lrow, "keep");
   fLayersKeep->Connect("Clicked()", "AlignConfigUI", this, "OnLayersKeep()");
   lrow->AddFrame(fLayersKeep, new TGLayoutHints(kLHintsCenterY, 0, 14, 3, 3));
   for (Int_t i = 0; i < 7; ++i) {
      fLayer[i] = new TGCheckButton(lrow, TString::Format("L%d", i).Data());
      fLayer[i]->Connect("Clicked()", "AlignConfigUI", this, "OnLayerToggle()");
      lrow->AddFrame(fLayer[i], new TGLayoutHints(kLHintsCenterY, 0, 8, 3, 3));
   }
   tab->AddFrame(lrow, new TGLayoutHints(kLHintsExpandX, 2, 2, 1, 1));
   fLayerInfo = new TGLabel(tab, "................................................................................");
   tab->AddFrame(fLayerInfo, new TGLayoutHints(kLHintsLeft, 158, 4, 0, 6));

   tab->AddFrame(new TGLabel(tab,
      "Method, DULEVEL and layers are patched into each worker's copy of the module; keep leaves the"),
      new TGLayoutHints(kLHintsLeft, 158, 4, 2, 0));
   tab->AddFrame(new TGLabel(tab,
      "archive's own value. The four below are written into each worker's YMLPParallel.h."),
      new TGLayoutHints(kLHintsLeft, 158, 4, 0, 6));

   struct { const char *label; TGNumberEntry **slot; Long_t min; Long_t max; } rows[] = {
      { "Events per worker",  &fEvents,    1, 100000000 },
      { "Epochs per step",    &fEpochs,    1, 10000 },
      { "jparallel",          &fJParallel, 0, 1000 },
      { "Cores per worker",   &fCores,     1, 256 },
      { 0, 0, 0, 0 }
   };
   for (Int_t i = 0; rows[i].label; ++i) {
      TGHorizontalFrame *r = new TGHorizontalFrame(tab);
      TGLabel *rl = new TGLabel(r, rows[i].label);
      rl->SetWidth(150);
      r->AddFrame(rl, new TGLayoutHints(kLHintsCenterY, 4, 6, 3, 3));
      *rows[i].slot = MakeNumber(r, rows[i].min, rows[i].max);
      r->AddFrame(*rows[i].slot, new TGLayoutHints(kLHintsCenterY, 0, 4, 3, 3));
      tab->AddFrame(r, new TGLayoutHints(kLHintsExpandX, 2, 2, 1, 1));
   }
}

void AlignConfigUI::BuildTuning(TGCompositeFrame *tab)
{
   fTuningTab = tab;
   tab->AddFrame(new TGLabel(tab,
      "DetectorConstant.h defines and YMultiLayerPerceptron.cxx constants, patched into each worker's copy."),
      new TGLayoutHints(kLHintsLeft, 158, 4, 6, 0));
   tab->AddFrame(new TGLabel(tab,
      "An empty field keeps the archive's own value, shown on the right once an archive has been read."),
      new TGLayoutHints(kLHintsLeft, 158, 4, 0, 6));

   for (Int_t i = 0; i < kNKnobs; ++i) {
      TGHorizontalFrame *r = new TGHorizontalFrame(tab);
      TGLabel *rl = new TGLabel(r, kKnobs[i].label);
      rl->SetWidth(150);
      r->AddFrame(rl, new TGLayoutHints(kLHintsCenterY, 4, 6, 2, 2));
      fKnob[i] = new TGTextEntry(r);
      fKnob[i]->Resize(140, 22);
      r->AddFrame(fKnob[i], new TGLayoutHints(kLHintsCenterY, 0, 10, 2, 2));
      fKnobArchive[i] = new TGLabel(r, "archive: ......................");
      r->AddFrame(fKnobArchive[i], new TGLayoutHints(kLHintsCenterY, 0, 4, 2, 2));
      tab->AddFrame(r, new TGLayoutHints(kLHintsExpandX, 2, 2, 0, 0));
   }

   TGHorizontalFrame *r = new TGHorizontalFrame(tab);
   TGLabel *rl = new TGLabel(r, "VERTEX_DERIVATIVES");
   rl->SetWidth(150);
   r->AddFrame(rl, new TGLayoutHints(kLHintsCenterY, 4, 6, 2, 2));
   fVertexDeriv = new TGComboBox(r);
   fVertexDeriv->AddEntry("keep", 0);
   fVertexDeriv->AddEntry("FALSE", 1);
   fVertexDeriv->AddEntry("TRUE", 2);
   fVertexDeriv->Resize(140, 22);
   r->AddFrame(fVertexDeriv, new TGLayoutHints(kLHintsCenterY, 0, 10, 2, 2));
   fVertexDerivArchive = new TGLabel(r, "archive: ......................");
   r->AddFrame(fVertexDerivArchive, new TGLayoutHints(kLHintsCenterY, 0, 4, 2, 2));
   tab->AddFrame(r, new TGLayoutHints(kLHintsExpandX, 2, 2, 0, 0));
}

void AlignConfigUI::BuildSchedule(TGCompositeFrame *tab)
{
   struct { const char *label; TGNumberEntry **slot; Long_t min; Long_t max; } rows[] = {
      { "Base step",        &fBaseStep,      0, 1000000 },
      { "Steps per batch",  &fStepsPerBatch, 1, 1000 },
      { "Batches",          &fNBatches,      1, 10000 },
      { "Workers",          &fNWorkers,      1, 200 },
      { "Batch ID",         &fBatchId,       0, 100000 },
      { 0, 0, 0, 0 }
   };
   for (Int_t i = 0; rows[i].label; ++i) {
      TGHorizontalFrame *r = new TGHorizontalFrame(tab);
      TGLabel *rl = new TGLabel(r, rows[i].label);
      rl->SetWidth(150);
      r->AddFrame(rl, new TGLayoutHints(kLHintsCenterY, 4, 6, 3, 3));
      *rows[i].slot = MakeNumber(r, rows[i].min, rows[i].max);
      (*rows[i].slot)->Connect("ValueSet(Long_t)", "AlignConfigUI", this, "UpdateSummary()");
      r->AddFrame(*rows[i].slot, new TGLayoutHints(kLHintsCenterY, 0, 4, 3, 3));
      tab->AddFrame(r, new TGLayoutHints(kLHintsExpandX, 2, 2, 1, 1));
   }

   fSummary = new TGLabel(tab, " ");
   tab->AddFrame(fSummary, new TGLayoutHints(kLHintsLeft, 158, 4, 14, 4));
   fSummaryHolder = tab;

   tab->AddFrame(new TGLabel(tab,
      "Workers is capped at 200 because WeightsMerge.C addresses that many slots (nPARALLEL)."),
      new TGLayoutHints(kLHintsLeft, 158, 4, 10, 2));
}

void AlignConfigUI::BuildEnvironment(TGCompositeFrame *tab)
{
   fO2Dir     = MakeRow(tab, "O2 installation", "OnBrowseO2()");
   fMasterDir = MakeRow(tab, "Data-prep macros", "OnBrowseMaster()");

   tab->AddFrame(new TGLabel(tab,
      "Leave the macro directory empty to use RUN/MasterDataScript. Set it only for a"),
      new TGLayoutHints(kLHintsLeft, 158, 4, 6, 0));
   tab->AddFrame(new TGLabel(tab,
      "tree laid out somewhere else. O2 is needed whatever the geometry backend: the data"),
      new TGLayoutHints(kLHintsLeft, 158, 4, 0, 0));
   tab->AddFrame(new TGLabel(tab,
      "split and the merge use it."),
      new TGLayoutHints(kLHintsLeft, 158, 4, 0, 8));

   TGHorizontalFrame *r = new TGHorizontalFrame(tab);
   TGLabel *rl = new TGLabel(r, "Launch stagger (s)");
   rl->SetWidth(150);
   r->AddFrame(rl, new TGLayoutHints(kLHintsCenterY, 4, 6, 3, 3));
   fStagger = MakeNumber(r, 0, 3600);
   r->AddFrame(fStagger, new TGLayoutHints(kLHintsCenterY, 0, 4, 3, 3));
   tab->AddFrame(r, new TGLayoutHints(kLHintsExpandX, 2, 2, 1, 1));

   fRunTag = MakeRow(tab, "Default run tag", 0);
}

// ---------------------------------------------------------------- loading ---

void AlignConfigUI::LoadAll()
{
   fDataDir->SetText(Get("DATA_INPUT_DIR"));
   fPattern->SetText(Get("DATA_FILE_PATTERN"));
   fFilesPerBatch->SetIntNumber(Get("DATA_FILES_PER_BATCH").Atoll());
   fMergeMax->SetIntNumber(Get("DATA_MERGE_MAX_FILES").Atoll());

   // Module archives actually present, plus whatever the file names, so a
   // configuration pointing at a missing archive still shows its own value.
   fModuleName->RemoveAll();
   TString current = Get("MODULE_NAME");
   Int_t id = 0, selected = -1;
   TString moduleDir = fRoot + "/MODULE";
   TSystemDirectory dir(moduleDir, moduleDir);
   TList *files = dir.GetListOfFiles();
   if (files) {
      files->Sort();
      TIter next(files);
      TSystemFile *f;
      while ((f = (TSystemFile *)next())) {
         TString name = f->GetName();
         if (f->IsDirectory() || !name.EndsWith(".tgz")) continue;
         name.Remove(name.Length() - 4);
         fModuleName->AddEntry(name, id);
         if (name == current) selected = id;
         id++;
      }
      files->Delete();
      delete files;
   }
   if (selected < 0 && !current.IsNull()) {
      fModuleName->AddEntry(current + "   (archive not found)", id);
      selected = id;
   }
   if (selected >= 0) fModuleName->Select(selected, kFALSE);

   // "auto" is id 0; a year is its own id. Anything else -- a hand-edited
   // value, or an error message from a broken file -- is left unselected
   // and shown in the log.
   TString schema = Get("TRACK_SCHEMA");
   if (schema == "auto")                                                       fTrackSchema->Select(0, kFALSE);
   else if (schema == "2024" || schema == "2025" || schema == "2026")          fTrackSchema->Select(schema.Atoi(), kFALSE);
   else Log(TString::Format("TRACK_SCHEMA is '%s' -- not a value this window knows; pick one before saving", schema.Data()));

   fEvents->SetIntNumber(Get("MODULE_EVENTS").Atoll());
   fEpochs->SetIntNumber(Get("MODULE_EPOCHS").Atoll());
   fJParallel->SetIntNumber(Get("MODULE_JPARALLEL").Atoll());
   fCores->SetIntNumber(Get("MODULE_CORES").Atoll());

   SelectByName(fGeomBackend, Get("GEOM_BACKEND"));
   SelectByName(fMethod, Get("MODULE_LEARNING_METHOD"));
   {
      // The DULEVEL entries carry the number first; match on that.
      TString want = Get("MODULE_DULEVEL");
      TGTextLBEntry *e;
      for (Int_t i = 0; (e = (TGTextLBEntry *)fDULevel->GetListBox()->GetEntry(i)); ++i) {
         TString text = e->GetText()->GetString();
         Ssiz_t sp = text.Index("(");
         if (sp > 0) text.Remove(sp);
         if (text == want) { fDULevel->Select(i, kFALSE); break; }
      }
   }
   {
      // Enable first, tick second: enabling a ticked box would untick it.
      TString layers = Get("MODULE_LAYERS");
      Bool_t keep = (layers == "keep");
      Enable(fLayersKeep, kTRUE);
      for (Int_t i = 0; i < 7; ++i) Enable(fLayer[i], kTRUE);
      fLayersKeep->SetState(keep ? kButtonDown : kButtonUp, kFALSE);
      SetLayerChecks(keep ? TString("") : layers);
      if (keep) for (Int_t i = 0; i < 7; ++i) Enable(fLayer[i], kFALSE);
   }

   for (Int_t i = 0; i < kNKnobs; ++i) {
      TString v = Get(kKnobs[i].key);
      fKnob[i]->SetText(v == "keep" ? "" : v.Data());
   }
   {
      TString v = Get("MODULE_VERTEX_DERIVATIVES");
      fVertexDeriv->Select(v == "TRUE" ? 2 : (v == "FALSE" ? 1 : 0), kFALSE);
   }

   fBaseStep->SetIntNumber(Get("BASE_STEP").Atoll());
   fStepsPerBatch->SetIntNumber(Get("STEPS_PER_BATCH").Atoll());
   fNBatches->SetIntNumber(Get("N_BATCHES").Atoll());
   fNWorkers->SetIntNumber(Get("N_WORKERS").Atoll());
   fBatchId->SetIntNumber(Get("BATCH_ID").Atoll());

   fO2Dir->SetText(Get("O2_DIR"));
   fMasterDir->SetText(Get("MASTER_DATA_SCRIPT_DIR"));
   fStagger->SetIntNumber(Get("WORKER_LAUNCH_STAGGER").Atoll());
   fRunTag->SetText(Get("RUN_TAG"));

   UpdateSummary();
   Rescan();
   Inspect();
   Log(TString::Format("loaded %s", fConf.Data()));
}

// Reads the selected archive through `alignctl.sh inspect`, fills the
// capability panel and greys out the knobs this module does not have. A
// failed read leaves everything enabled: the driver checks again anyway.
void AlignConfigUI::Inspect()
{
   fCapMap.clear();
   fCapKnown = kFALSE;

   TString name = SelectedArchive();
   TString archive = fRoot + "/MODULE/" + name + ".tgz";
   if (name.IsNull() || gSystem->AccessPathName(archive, kReadPermission)) {
      fCap[0]->SetText("no archive selected, or it is not in MODULE/ -- capabilities unknown, every knob left enabled");
      for (Int_t i = 1; i < 6; ++i) fCap[i]->SetText(" ");
      ApplyCapabilities();
      return;
   }

   TString out = Run(TString::Format("inspect %s", Quote(archive).Data()));
   TObjArray *lines = out.Tokenize("\n");
   for (Int_t i = 0; i < lines->GetEntries(); ++i) {
      TString line = ((TObjString *)lines->At(i))->GetString();
      Ssiz_t eq = line.Index("=");
      if (eq <= 0 || !line.BeginsWith("MP_")) continue;
      // TSubString::Data() is not NUL-terminated at the substring's end;
      // go through a TString to get the key and the value on their own.
      TString key(line(0, eq));
      TString val(line(eq + 1, line.Length() - eq - 1));
      fCapMap[std::string(key.Data())] = std::string(val.Data());
   }
   delete lines;

   if (!CapIs("MP_VALID", "1")) {
      LogCommand("inspect", out);
      fCap[0]->SetText(TString::Format("could not read %s.tgz -- see the log", name.Data()));
      fCap[1]->SetText("capabilities unknown -- every knob left enabled; the driver checks again before launching");
      for (Int_t i = 2; i < 6; ++i) fCap[i]->SetText(" ");
      fCapMap.clear();
      ApplyCapabilities();
      return;
   }
   fCapKnown = kTRUE;

   fCap[0]->SetText(TString::Format("generation %s -- input schema %s (%s), unpacks to %s/%s",
      Cap("MP_GENERATION").Data(), Cap("MP_SCHEMA").Data(),
      CapIs("MP_HAS_CHARGE", "1") ? "per-track charge" : "no charge",
      Cap("MP_TOP").Data(), CapIs("MP_TOP_OK", "1") ? "" : "  -- NOT the archive name; the driver will refuse it"));
   fCap[1]->SetText(TString::Format("learning methods implemented: %s   (driver macro ships %s)",
      Cap("MP_METHODS_IMPL").Data(), Cap("MP_MOD_METHOD").Data()));
   fCap[2]->SetText(TString::Format("detector unit: %s   layer selection: %s   adaptive vertex: %s",
      CapIs("MP_DETECTOR_UNIT", "1") ? TString::Format("yes (DULEVEL %s)", Cap("MP_MOD_DULEVEL").Data()).Data() : "no (per chip)",
      CapIs("MP_LAYER_SELECT", "1") ? TString::Format("yes (mask %s)", Cap("MP_MOD_LAYER_MASK").Data()).Data() : "no (all layers)",
      CapIs("MP_ADAPTIVE_VERTEX", "1") ? "yes" : "no"));
   if (CapIs("MP_CACHE_CAPABLE", "1"))
      fCap[3]->SetText(TString::Format("geometry: o2 or cache (ships as %s); cache file in the archive: %s",
         Cap("MP_GEOM_SHIPPED").Data(), CapIs("MP_CACHE_FILE", "1") ? "yes" : "no -- cache mode needs a repack"));
   else
      fCap[3]->SetText("geometry: O2 only");
   fCap[4]->SetText(TString::Format("archive holds nDATA %s, nEPOCH %s, nTrackMax %s, DET_MAG %s T; weightsDU.txt %s",
      Cap("MP_MOD_NDATA").Data(), Cap("MP_MOD_NEPOCH").Data(), Cap("MP_MOD_NTRACKMAX").Data(),
      Cap("MP_MOD_DET_MAG").Data(), CapIs("MP_DU_PERSIST", "1") ? "reloaded each step" : "ignored"));
   fCap[5]->SetText("knobs this module does not have are greyed out and saved as keep");

   ApplyCapabilities();
}

// Enables the knobs the inspected archive has and parks the others at the
// value that leaves the archive alone. Combos always keep a selection, so
// Save always has something to send.
void AlignConfigUI::ApplyCapabilities()
{
   Bool_t cache  = !fCapKnown || CapIs("MP_CACHE_CAPABLE", "1");
   Bool_t du     = !fCapKnown || CapIs("MP_DETECTOR_UNIT", "1");
   Bool_t layers = !fCapKnown || CapIs("MP_LAYER_SELECT", "1");
   Bool_t vertex = !fCapKnown || CapIs("MP_ADAPTIVE_VERTEX", "1");
   Bool_t vderiv = !fCapKnown || !Cap("MP_MOD_VERTEX_DERIVATIVES").IsNull();

   if (!cache) SelectByName(fGeomBackend, "o2");
   fGeomBackend->SetEnabled(cache);

   if (!du) SelectByName(fDULevel, "keep");
   fDULevel->SetEnabled(du);

   fLayersOn = layers;
   if (!layers) {
      Enable(fLayersKeep, kTRUE);
      fLayersKeep->SetState(kButtonDown, kFALSE);
      for (Int_t i = 0; i < 7; ++i) Enable(fLayer[i], kFALSE);
   }
   Enable(fLayersKeep, layers);
   if (layers && !fLayersKeep->IsOn())
      for (Int_t i = 0; i < 7; ++i) Enable(fLayer[i], kTRUE);

   for (Int_t i = 0; i < kNKnobs; ++i) {
      TString key = kKnobs[i].key;
      Bool_t on = kTRUE;
      if (key.BeginsWith("MODULE_QUALITY_") || key == "MODULE_MAX_BAD_TRACKS") on = vertex;
      if (!on) fKnob[i]->SetText("");
      fKnobOn[i] = on;
      fKnob[i]->SetEnabled(on);
      TString have = fCapKnown ? Cap(kKnobs[i].mpkey) : TString("");
      if (!fCapKnown)          fKnobArchive[i]->SetText("archive: not read");
      else if (have.IsNull())  fKnobArchive[i]->SetText("archive: not in this module");
      else                     fKnobArchive[i]->SetText(TString::Format("archive: %s", have.Data()));
   }
   if (!vderiv) fVertexDeriv->Select(0, kFALSE);
   fVertexDerivOn = vderiv;
   fVertexDeriv->SetEnabled(vderiv);
   {
      TString have = fCapKnown ? Cap("MP_MOD_VERTEX_DERIVATIVES") : TString("");
      if (!fCapKnown)         fVertexDerivArchive->SetText("archive: not read");
      else if (have.IsNull()) fVertexDerivArchive->SetText("archive: not in this module");
      else                    fVertexDerivArchive->SetText(TString::Format("archive: %s", have.Data()));
   }

   UpdateLayerInfo();
   // TGLabel::SetText does not resize the label's frame; the holders must be
   // laid out again or the new text is clipped to the old width.
   fCapFrame->Layout();
   fModuleTab->Layout();
   fTuningTab->Layout();
}

void AlignConfigUI::UpdateSummary()
{
   Long_t base    = fBaseStep->GetIntNumber();
   Long_t perB    = fStepsPerBatch->GetIntNumber();
   Long_t batches = fNBatches->GetIntNumber();
   Long_t workers = fNWorkers->GetIntNumber();
   Long_t total   = perB * batches;
   fSummary->SetText(TString::Format(
      "steps %ld to %ld  |  %ld steps total  |  %ld module runs  |  %ld merges",
      base + 1, base + total, total, batches * workers, batches));
   fSummaryHolder->Layout();
}

TString AlignConfigUI::LayerList() const
{
   TString out;
   for (Int_t l = 0; l < 7; ++l)
      if (fLayer[l]->IsOn()) {
         if (out.Length()) out += ",";
         out += l;
      }
   return out;
}

void AlignConfigUI::SetLayerChecks(const TString &list)
{
   Bool_t on[7] = { kFALSE, kFALSE, kFALSE, kFALSE, kFALSE, kFALSE, kFALSE };
   TObjArray *parts = list.Tokenize(",");
   for (Int_t i = 0; i < parts->GetEntries(); ++i) {
      TString t = ((TObjString *)parts->At(i))->GetString();
      t.ReplaceAll(" ", "");
      if (t.Length() == 1 && t[0] >= '0' && t[0] <= '6') on[t[0] - '0'] = kTRUE;
   }
   delete parts;
   for (Int_t l = 0; l < 7; ++l)
      fLayer[l]->SetState(on[l] ? kButtonDown : kButtonUp, kFALSE);
}

void AlignConfigUI::UpdateLayerInfo()
{
   static const Int_t chipsInLayer[7] = { 108, 144, 180, 2688, 3360, 8232, 9408 };
   if (fLayersKeep->IsOn() || !fLayersOn) {
      TString mask = fCapKnown ? Cap("MP_MOD_LAYER_MASK") : TString("");
      if (fCapKnown && !CapIs("MP_LAYER_SELECT", "1"))
         fLayerInfo->SetText("this module has no layer selection: its update covers all seven layers");
      else if (!mask.IsNull())
         fLayerInfo->SetText(TString::Format("keep: the archive's own ALIGN_LAYER_MASK %s", mask.Data()));
      else
         fLayerInfo->SetText("keep: the archive's own layer selection");
      return;
   }
   Int_t chips = 0, mask = 0;
   for (Int_t l = 0; l < 7; ++l)
      if (fLayer[l]->IsOn()) { chips += chipsInLayer[l]; mask |= (1 << l); }
   if (!mask) {
      fLayerInfo->SetText("no layer ticked -- tick at least one, or keep");
      return;
   }
   fLayerInfo->SetText(TString::Format("%d of 24120 chips aligned, ALIGN_LAYER_MASK 0x%02X (batch update: kBatch, kSteepestDescent; kStochastic ignores it)",
                                       chips, mask));
}

// Lists the input directory and ticks the files the configuration selects.
void AlignConfigUI::Rescan()
{
   // DeleteChildren() dereferences the item it is given, so a null "root"
   // is not a way to empty the tree; remove the top-level items instead.
   TGListTreeItem *old = fFileTree->GetFirstItem();
   while (old) {
      TGListTreeItem *nextItem = old->GetNextSibling();
      fFileTree->DeleteItem(old);
      old = nextItem;
   }

   TString dirName = fDataDir->GetText();
   if (dirName.IsNull()) { fFileCount->SetText("no input directory set"); return; }
   if (gSystem->AccessPathName(dirName, kReadPermission)) {
      fFileCount->SetText("input directory is not readable");
      fClient->NeedRedraw(fFileTree);
      return;
   }

   TString selected = TString(" ") + Get("DATA_FILES") + " ";
   selected.ReplaceAll("\n", " ");
   selected.ReplaceAll("\t", " ");

   TString pat = fPattern->GetText();
   if (pat.IsNull()) pat = "*";
   TRegexp re(pat, kTRUE);

   TSystemDirectory dir(dirName, dirName);
   TList *files = dir.GetListOfFiles();
   Int_t shown = 0, ticked = 0;
   if (files) {
      files->Sort();
      TIter next(files);
      TSystemFile *f;
      while ((f = (TSystemFile *)next())) {
         if (f->IsDirectory()) continue;
         TString name = f->GetName();
         Bool_t on = selected.Contains(TString(" ") + name + " ");
         Ssiz_t len = 0;
         Bool_t matches = (name.Index(re, &len) == 0 && len == name.Length());
         // Show anything the pattern matches, and anything already configured
         // even if it does not. Save reads the tree, so a configured file left
         // out of the list here would be dropped from the configuration.
         if (!matches && !on) continue;

         TGListTreeItem *item = fFileTree->AddItem(0, name);
         fFileTree->SetCheckBox(item, kTRUE);
         fFileTree->CheckItem(item, on);
         if (on) ticked++;
         shown++;
      }
      files->Delete();
      delete files;
   }
   Int_t configured = 0;
   {
      TObjArray *names = selected.Tokenize(" ");
      configured = names->GetEntries();
      delete names;
   }
   if (configured > ticked) {
      fFileCount->SetText(TString::Format(
         "%d listed, %d selected -- %d configured file(s) are NOT in this directory "
         "and saving will drop them", shown, ticked, configured - ticked));
   } else {
      fFileCount->SetText(TString::Format("%d file(s) listed, %d selected", shown, ticked));
   }
   fFileCountRow->Layout();
   fClient->NeedRedraw(fFileTree);
}

// ------------------------------------------------------------------ slots ---

void AlignConfigUI::OnBrowseData()
{
   TString start = fDataDir->GetText();
   TString picked = DirBrowser::Pick(gClient->GetRoot(), start.Data());
   if (!picked.IsNull()) { fDataDir->SetText(picked); Rescan(); }
}

void AlignConfigUI::OnBrowseO2()
{
   TString picked = DirBrowser::Pick(gClient->GetRoot(), fO2Dir->GetText());
   if (!picked.IsNull()) fO2Dir->SetText(picked);
}

void AlignConfigUI::OnBrowseMaster()
{
   TString start = fMasterDir->GetText();
   if (start.IsNull()) start = fRoot + "/RUN";
   TString picked = DirBrowser::Pick(gClient->GetRoot(), start.Data());
   if (!picked.IsNull()) fMasterDir->SetText(picked);
}

// A different archive was picked: read it, so the panel and the greyed-out
// knobs describe the archive that will be saved, not the previous one.
void AlignConfigUI::OnModuleSelected() { Inspect(); }

void AlignConfigUI::OnLayersKeep()
{
   Bool_t keep = fLayersKeep->IsOn();
   for (Int_t i = 0; i < 7; ++i) Enable(fLayer[i], !keep);
   UpdateLayerInfo();
   fModuleTab->Layout();
}

void AlignConfigUI::OnLayerToggle()
{
   UpdateLayerInfo();
   fModuleTab->Layout();
}

void AlignConfigUI::OnRescan()  { Rescan(); }
void AlignConfigUI::OnReload()  { LoadAll(); }

void AlignConfigUI::OnValidate() { LogCommand("validate", Run("validate")); }
void AlignConfigUI::OnDoctor()   { LogCommand("check machine", Run("doctor")); }
void AlignConfigUI::OnGenerate() { LogCommand("generate", Run("generate")); }

void AlignConfigUI::OnSave()
{
   // Collect the ticked files first; an empty selection is a mistake worth
   // stopping for rather than writing out.
   TString chosen;
   Int_t n = 0;
   for (TGListTreeItem *it = fFileTree->GetFirstItem(); it; it = it->GetNextSibling()) {
      if (!it->IsChecked()) continue;
      if (n++) chosen += " ";
      chosen += it->GetText();
   }
   if (n == 0) {
      new TGMsgBox(gClient->GetRoot(), this, "Nothing selected",
                   "No input files are ticked. Tick at least one before saving.",
                   kMBIconExclamation, kMBOk);
      return;
   }

   TString moduleName = SelectedArchive();
   if (moduleName.IsNull()) {
      new TGMsgBox(gClient->GetRoot(), this, "No module",
                   "Pick a module archive on the Module tab before saving.",
                   kMBIconExclamation, kMBOk);
      return;
   }

   // The schema combo carries ids, the file carries words.
   Int_t schemaId = fTrackSchema->GetSelected();
   if (schemaId < 0) {
      new TGMsgBox(gClient->GetRoot(), this, "No track schema",
                   "Pick a track schema on the Module tab before saving.",
                   kMBIconExclamation, kMBOk);
      return;
   }
   TString schema = schemaId == 0 ? TString("auto") : TString::Format("%d", schemaId);

   // Layers: keep when the box says so or the module has none; otherwise
   // the ticked list, which must not be empty. Disabled boxes read as off,
   // so the keep state is consulted first.
   TString layers;
   if (fLayersKeep->IsOn() || !fLayersOn) {
      layers = "keep";
   } else {
      layers = LayerList();
      if (layers.IsNull()) {
         new TGMsgBox(gClient->GetRoot(), this, "No layer",
                      "No layer is ticked. Tick at least one, or tick keep.",
                      kMBIconExclamation, kMBOk);
         return;
      }
   }

   TString args = "set";
   args += " DATA_INPUT_DIR=" + Quote(fDataDir->GetText());
   args += " DATA_FILE_PATTERN=" + Quote(fPattern->GetText());
   args += " DATA_FILES=" + Quote(chosen);
   args += TString::Format(" DATA_FILES_PER_BATCH=%lld", (Long64_t)fFilesPerBatch->GetIntNumber());
   args += TString::Format(" DATA_MERGE_MAX_FILES=%lld", (Long64_t)fMergeMax->GetIntNumber());
   args += " MODULE_NAME=" + Quote(moduleName);
   args += " TRACK_SCHEMA=" + Quote(schema);
   args += TString::Format(" MODULE_EVENTS=%lld",    (Long64_t)fEvents->GetIntNumber());
   args += TString::Format(" MODULE_EPOCHS=%lld",    (Long64_t)fEpochs->GetIntNumber());
   args += TString::Format(" MODULE_JPARALLEL=%lld", (Long64_t)fJParallel->GetIntNumber());
   args += TString::Format(" MODULE_CORES=%lld",     (Long64_t)fCores->GetIntNumber());
   args += " GEOM_BACKEND=" + Quote(SelectedName(fGeomBackend));
   args += " MODULE_LEARNING_METHOD=" + Quote(SelectedName(fMethod));
   {
      TString v = SelectedName(fDULevel);
      Ssiz_t sp = v.Index("(");
      if (sp > 0) v.Remove(sp);
      args += " MODULE_DULEVEL=" + Quote(v);
   }
   args += " MODULE_LAYERS=" + Quote(layers);
   for (Int_t i = 0; i < kNKnobs; ++i) {
      // An empty or disabled entry is keep; validation rejects anything else
      // that is not a number, with the whole save held back.
      TString v = fKnobOn[i] ? TString(fKnob[i]->GetText()) : TString("");
      v = v.Strip(TString::kBoth);
      args += TString::Format(" %s=", kKnobs[i].key) + Quote(v.IsNull() ? TString("keep") : v);
   }
   args += " MODULE_VERTEX_DERIVATIVES=" + Quote(fVertexDerivOn ? SelectedName(fVertexDeriv) : TString("keep"));
   args += TString::Format(" BASE_STEP=%lld",        (Long64_t)fBaseStep->GetIntNumber());
   args += TString::Format(" STEPS_PER_BATCH=%lld",  (Long64_t)fStepsPerBatch->GetIntNumber());
   args += TString::Format(" N_BATCHES=%lld",        (Long64_t)fNBatches->GetIntNumber());
   args += TString::Format(" N_WORKERS=%lld",        (Long64_t)fNWorkers->GetIntNumber());
   args += TString::Format(" BATCH_ID=%lld",         (Long64_t)fBatchId->GetIntNumber());
   args += " O2_DIR=" + Quote(fO2Dir->GetText());
   args += " MASTER_DATA_SCRIPT_DIR=" + Quote(fMasterDir->GetText());
   args += TString::Format(" WORKER_LAUNCH_STAGGER=%lld", (Long64_t)fStagger->GetIntNumber());
   args += " RUN_TAG=" + Quote(fRunTag->GetText());

   LogCommand("save", Run(args));
   LogCommand("generate", Run("generate"));
   Rescan();
   Inspect();
}

void AlignConfigUI::OnQuit()
{
   UnmapWindow();
   CloseWindow();
   if (gApplication) gApplication->Terminate(0);
}

// --------------------------------------------------------------- entry point ---

void ConfigUI(const char *conf = "config/alignment.conf")
{
   new AlignConfigUI(gClient->GetRoot(), conf);
}
