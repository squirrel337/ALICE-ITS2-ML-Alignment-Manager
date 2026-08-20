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
//  one place and the GUI and the command line cannot drift apart.
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

   TGComboBox    *fModuleName;
   TGComboBox    *fTrackSchema;
   TGNumberEntry *fEvents;
   TGNumberEntry *fEpochs;
   TGNumberEntry *fJParallel;
   TGNumberEntry *fCores;

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

   TString Run(const char *args);
   TString Get(const char *key);
   static TString Quote(const TString &s);
   void Log(const TString &text);
   void LogCommand(const TString &title, const TString &output);

   TGNumberEntry *MakeNumber(TGCompositeFrame *p, Long_t min, Long_t max);
   TGTextEntry   *MakeRow(TGCompositeFrame *p, const char *label, const char *slot);

   void BuildData(TGCompositeFrame *tab);
   void BuildModule(TGCompositeFrame *tab);
   void BuildSchedule(TGCompositeFrame *tab);
   void BuildEnvironment(TGCompositeFrame *tab);

public:
   AlignConfigUI(const TGWindow *p, const char *conf);
   virtual ~AlignConfigUI() { Cleanup(); }

   void LoadAll();
   void Rescan();
   void UpdateSummary();

   void OnBrowseData();
   void OnBrowseO2();
   void OnBrowseMaster();
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

// ------------------------------------------------------------ construction ---

AlignConfigUI::AlignConfigUI(const TGWindow *p, const char *conf)
   : TGMainFrame(p, 900, 700)
{
   SetCleanup(kDeepCleanup);
   SetWindowName("ITS2 Alignment Configuration");

   fConf = gSystem->ExpandPathName(conf);
   if (!fConf.BeginsWith("/"))
      fConf = TString::Format("%s/%s", gSystem->WorkingDirectory(), fConf.Data());
   TString confDir = gSystem->DirName(fConf);
   fCtl  = confDir + "/alignctl.sh";
   fRoot = gSystem->DirName(confDir);

   TGTab *tabs = new TGTab(this, 880, 480);
   BuildData(tabs->AddTab("Data"));
   BuildModule(tabs->AddTab("Module"));
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

   fLog = new TGTextView(this, 880, 150);
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

   fFileCanvas = new TGCanvas(tab, 840, 220);
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
   TGHorizontalFrame *row = new TGHorizontalFrame(tab);
   TGLabel *l = new TGLabel(row, "Module archive");
   l->SetWidth(150);
   row->AddFrame(l, new TGLayoutHints(kLHintsCenterY, 4, 6, 4, 4));
   fModuleName = new TGComboBox(row);
   fModuleName->Resize(560, 22);
   row->AddFrame(fModuleName, new TGLayoutHints(kLHintsExpandX | kLHintsCenterY, 0, 4, 4, 4));
   tab->AddFrame(row, new TGLayoutHints(kLHintsExpandX, 2, 2, 4, 4));

   TGHorizontalFrame *schemaRow = new TGHorizontalFrame(tab);
   TGLabel *sl = new TGLabel(schemaRow, "Track schema");
   sl->SetWidth(150);
   schemaRow->AddFrame(sl, new TGLayoutHints(kLHintsCenterY, 4, 6, 4, 4));
   fTrackSchema = new TGComboBox(schemaRow);
   fTrackSchema->AddEntry("2024   (no per-track charge)", 2024);
   fTrackSchema->AddEntry("2025   (per-track charge)", 2025);
   fTrackSchema->Resize(260, 22);
   schemaRow->AddFrame(fTrackSchema, new TGLayoutHints(kLHintsCenterY, 0, 4, 4, 4));
   tab->AddFrame(schemaRow, new TGLayoutHints(kLHintsExpandX, 2, 2, 0, 4));

   tab->AddFrame(new TGLabel(tab,
      "The 2025 input tree carries a per-track charge and the 2024 tree does not, so this"),
      new TGLayoutHints(kLHintsLeft, 158, 4, 0, 0));
   tab->AddFrame(new TGLabel(tab,
      "must match the archive above. Check machine reads the archive and verifies it."),
      new TGLayoutHints(kLHintsLeft, 158, 4, 0, 8));

   tab->AddFrame(new TGLabel(tab,
      "These four are #defines inside the archive. They are written into each worker's"),
      new TGLayoutHints(kLHintsLeft, 158, 4, 8, 0));
   tab->AddFrame(new TGLabel(tab,
      "YMLPParallel.h after the module is unpacked, so the archive itself stays untouched."),
      new TGLayoutHints(kLHintsLeft, 158, 4, 0, 8));

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
      "tree laid out somewhere else."),
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

   fTrackSchema->Select(Get("TRACK_SCHEMA").Atoi(), kFALSE);

   fEvents->SetIntNumber(Get("MODULE_EVENTS").Atoll());
   fEpochs->SetIntNumber(Get("MODULE_EPOCHS").Atoll());
   fJParallel->SetIntNumber(Get("MODULE_JPARALLEL").Atoll());
   fCores->SetIntNumber(Get("MODULE_CORES").Atoll());

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
   Log(TString::Format("loaded %s", fConf.Data()));
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

   TString moduleName;
   TGTextLBEntry *sel = (TGTextLBEntry *)fModuleName->GetSelectedEntry();
   if (sel) {
      moduleName = sel->GetText()->GetString();
      moduleName.ReplaceAll("   (archive not found)", "");
   }

   if (moduleName.IsNull()) {
      new TGMsgBox(gClient->GetRoot(), this, "No module",
                   "Pick a module archive on the Module tab before saving.",
                   kMBIconExclamation, kMBOk);
      return;
   }

   TString args = "set";
   args += " DATA_INPUT_DIR=" + Quote(fDataDir->GetText());
   args += " DATA_FILE_PATTERN=" + Quote(fPattern->GetText());
   args += " DATA_FILES=" + Quote(chosen);
   args += TString::Format(" DATA_FILES_PER_BATCH=%lld", (Long64_t)fFilesPerBatch->GetIntNumber());
   args += TString::Format(" DATA_MERGE_MAX_FILES=%lld", (Long64_t)fMergeMax->GetIntNumber());
   args += " MODULE_NAME=" + Quote(moduleName);
   args += TString::Format(" TRACK_SCHEMA=%d", fTrackSchema->GetSelected());
   args += TString::Format(" MODULE_EVENTS=%lld",    (Long64_t)fEvents->GetIntNumber());
   args += TString::Format(" MODULE_EPOCHS=%lld",    (Long64_t)fEpochs->GetIntNumber());
   args += TString::Format(" MODULE_JPARALLEL=%lld", (Long64_t)fJParallel->GetIntNumber());
   args += TString::Format(" MODULE_CORES=%lld",     (Long64_t)fCores->GetIntNumber());
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
