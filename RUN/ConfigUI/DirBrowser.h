// ==========================================================================
//  DirBrowser -- a TBrowser-style directory chooser
// ==========================================================================
//  ROOT's TGFileDialog picks files, not directories, so the alignment
//  configuration window carries its own chooser: a lazily populated tree of
//  subdirectories with Select and Cancel.
//
//  Modal. Use it as:
//     TString picked = DirBrowser::Pick(gClient->GetRoot(), start);
//  which returns an empty string if the user cancelled.
// ==========================================================================

#ifndef DIRBROWSER_H
#define DIRBROWSER_H

#include <TGFrame.h>
#include <TGListTree.h>
#include <TGCanvas.h>
#include <TGButton.h>
#include <TGLabel.h>
#include <TGTextEntry.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TList.h>
#include <TString.h>
#include <TObjArray.h>
#include <TObjString.h>

class DirBrowser : public TGTransientFrame {
private:
   TGListTree   *fTree;
   TGTextEntry  *fPath;
   TString      *fResult;      // written on Select, owned by the caller
   Bool_t        fDone;

   // Children are filled one level at a time: a node gets its own children
   // when it is created, so the expander arrow is correct, and its
   // grandchildren when it is opened.
   void Fill(TGListTreeItem *node, const char *path, Int_t depth);
   TString PathOf(TGListTreeItem *item) const;

public:
   DirBrowser(const TGWindow *p, const char *start, TString *result);
   virtual ~DirBrowser();

   void OnOpen(TGListTreeItem *item, Int_t btn);
   void OnSelect();
   void OnCancel();
   void OnGoTo();

   static TString Pick(const TGWindow *p, const char *start);

   ClassDef(DirBrowser, 0)
};

//---------------------------------------------------------------------------

DirBrowser::DirBrowser(const TGWindow *p, const char *start, TString *result)
   : TGTransientFrame(p, p, 520, 460), fTree(0), fPath(0), fResult(result), fDone(kFALSE)
{
   SetCleanup(kDeepCleanup);
   SetWindowName("Choose a directory");

   TGCanvas *canvas = new TGCanvas(this, 500, 340);
   fTree = new TGListTree(canvas, kHorizontalFrame);
   fTree->Connect("DoubleClicked(TGListTreeItem*, Int_t)", "DirBrowser", this,
                  "OnOpen(TGListTreeItem*, Int_t)");
   canvas->SetContainer(fTree);
   AddFrame(canvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 4, 4, 4, 2));

   TGHorizontalFrame *bar = new TGHorizontalFrame(this);
   bar->AddFrame(new TGLabel(bar, "Path"), new TGLayoutHints(kLHintsCenterY, 4, 4, 2, 2));
   fPath = new TGTextEntry(bar);
   fPath->SetText(start && start[0] ? start : "/");
   bar->AddFrame(fPath, new TGLayoutHints(kLHintsExpandX | kLHintsCenterY, 0, 4, 2, 2));
   TGTextButton *go = new TGTextButton(bar, " Go ");
   go->Connect("Clicked()", "DirBrowser", this, "OnGoTo()");
   bar->AddFrame(go, new TGLayoutHints(kLHintsCenterY, 0, 4, 2, 2));
   AddFrame(bar, new TGLayoutHints(kLHintsExpandX, 4, 4, 2, 2));

   TGHorizontalFrame *buttons = new TGHorizontalFrame(this);
   TGTextButton *ok = new TGTextButton(buttons, " Select ");
   ok->Connect("Clicked()", "DirBrowser", this, "OnSelect()");
   buttons->AddFrame(ok, new TGLayoutHints(kLHintsRight, 4, 4, 4, 4));
   TGTextButton *cancel = new TGTextButton(buttons, " Cancel ");
   cancel->Connect("Clicked()", "DirBrowser", this, "OnCancel()");
   buttons->AddFrame(cancel, new TGLayoutHints(kLHintsRight, 4, 4, 4, 4));
   AddFrame(buttons, new TGLayoutHints(kLHintsRight, 4, 4, 0, 4));

   // Root the tree at "/" and walk down to the starting directory so the
   // user opens on something familiar.
   TGListTreeItem *root = fTree->AddItem(0, "/");
   root->SetUserData((void *)StrDup("/"));
   Fill(root, "/", 1);
   fTree->OpenItem(root);

   MapSubwindows();
   Resize(GetDefaultSize());
   CenterOnParent();
   MapWindow();
   fClient->WaitFor(this);
}

DirBrowser::~DirBrowser() { Cleanup(); }

TString DirBrowser::PathOf(TGListTreeItem *item) const
{
   if (!item) return TString();
   const char *ud = (const char *)item->GetUserData();
   return TString(ud ? ud : "");
}

void DirBrowser::Fill(TGListTreeItem *node, const char *path, Int_t depth)
{
   if (depth <= 0 || !node) return;
   if (node->GetFirstChild() && depth == 1) return;   // already populated

   TSystemDirectory dir(path, path);
   TList *entries = dir.GetListOfFiles();
   if (!entries) return;                              // unreadable; leave it a leaf

   entries->Sort();
   TIter next(entries);
   TSystemFile *f;
   Int_t added = 0;
   while ((f = (TSystemFile *)next())) {
      TString name = f->GetName();
      if (!f->IsDirectory()) continue;
      if (name == "." || name == "..") continue;

      TString full = path;
      if (!full.EndsWith("/")) full += "/";
      full += name;

      // Skip an existing child with the same name, so re-opening a node
      // does not duplicate its contents.
      Bool_t seen = kFALSE;
      for (TGListTreeItem *c = node->GetFirstChild(); c; c = c->GetNextSibling())
         if (name == c->GetText()) { seen = kTRUE; break; }
      if (seen) continue;

      TGListTreeItem *child = fTree->AddItem(node, name);
      child->SetUserData((void *)StrDup(full));
      Fill(child, full, depth - 1);
      if (++added > 2000) break;                      // guard against huge trees
   }
   delete entries;
}

void DirBrowser::OnOpen(TGListTreeItem *item, Int_t)
{
   if (!item) return;
   TString path = PathOf(item);
   if (path.IsNull()) return;
   Fill(item, path, 1);
   for (TGListTreeItem *c = item->GetFirstChild(); c; c = c->GetNextSibling())
      Fill(c, PathOf(c), 1);
   fPath->SetText(path);
   fClient->NeedRedraw(fTree);
}

void DirBrowser::OnGoTo()
{
   TString want = fPath->GetText();
   if (want.IsNull()) return;
   if (gSystem->AccessPathName(want, kReadPermission)) {
      fPath->SetText(TString::Format("%s  <-- not readable", want.Data()));
      return;
   }
   // Walk from the root, opening each component, so the tree ends up showing
   // the directory that was typed.
   TGListTreeItem *node = fTree->GetFirstItem();
   Fill(node, "/", 1);
   fTree->OpenItem(node);

   TString rest = want;
   rest = rest.Strip(TString::kLeading, '/');
   TString acc = "";
   TObjArray *parts = rest.Tokenize("/");
   for (Int_t i = 0; i < parts->GetEntries(); ++i) {
      TString part = ((TObjString *)parts->At(i))->GetString();
      if (part.IsNull()) continue;
      acc += "/" + part;
      TGListTreeItem *child = 0;
      for (TGListTreeItem *c = node->GetFirstChild(); c; c = c->GetNextSibling())
         if (part == c->GetText()) { child = c; break; }
      if (!child) break;
      node = child;
      Fill(node, acc, 1);
      fTree->OpenItem(node);
   }
   delete parts;
   fTree->SetSelected(node);
   fClient->NeedRedraw(fTree);
}

void DirBrowser::OnSelect()
{
   TGListTreeItem *sel = fTree->GetSelected();
   TString path = sel ? PathOf(sel) : TString(fPath->GetText());
   if (path.IsNull()) path = fPath->GetText();
   if (fResult) *fResult = path;
   fDone = kTRUE;
   UnmapWindow();
   CloseWindow();
}

void DirBrowser::OnCancel()
{
   if (fResult) *fResult = "";
   fDone = kTRUE;
   UnmapWindow();
   CloseWindow();
}

TString DirBrowser::Pick(const TGWindow *p, const char *start)
{
   TString result;
   new DirBrowser(p, start, &result);      // modal; deletes itself on close
   return result;
}

#endif
