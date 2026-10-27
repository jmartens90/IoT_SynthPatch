Procedure ExportBoard;
Var
  Board : IPCB_Board;
  Iter  : IPCB_BoardIterator;
  Comp  : IPCB_Component;
  Pad   : IPCB_Pad;
  L     : TStringList;
  S, NetName, CompName : String;
  P     : String;
Begin
  Board := PCBServer.GetCurrentPCBBoard;
  If Board = Nil Then Exit;
  L := TStringList.Create;
  Iter := Board.BoardIterator_Create;
  Iter.AddFilter_ObjectSet(MkSet(eComponentObject));
  Iter.AddFilter_LayerSet(AllLayers);
  Iter.AddFilter_Method(eProcessAll);
  Comp := Iter.FirstPCBObject;
  While Comp <> Nil Do
  Begin
    S := 'C;' + Comp.Name.Text + ';' + Comp.Pattern + ';' +
         FloatToStr(CoordToMMs(Comp.X - Board.XOrigin)) + ';' +
         FloatToStr(CoordToMMs(Comp.Y - Board.YOrigin)) + ';' +
         FloatToStr(Comp.Rotation) + ';' + IntToStr(Comp.Layer);
    L.Add(S);
    Comp := Iter.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(Iter);
  Iter := Board.BoardIterator_Create;
  Iter.AddFilter_ObjectSet(MkSet(ePadObject));
  Iter.AddFilter_LayerSet(AllLayers);
  Iter.AddFilter_Method(eProcessAll);
  Pad := Iter.FirstPCBObject;
  While Pad <> Nil Do
  Begin
    NetName := '';
    If Pad.Net <> Nil Then NetName := Pad.Net.Name;
    CompName := '';
    If Pad.Component <> Nil Then CompName := Pad.Component.Name.Text;
    S := 'P;' + CompName + ';' + Pad.Name + ';' + NetName + ';' +
         FloatToStr(CoordToMMs(Pad.X - Board.XOrigin)) + ';' +
         FloatToStr(CoordToMMs(Pad.Y - Board.YOrigin)) + ';' +
         FloatToStr(CoordToMMs(Pad.TopXSize)) + ';' + FloatToStr(CoordToMMs(Pad.TopYSize)) + ';' +
         FloatToStr(CoordToMMs(Pad.HoleSize)) + ';' + IntToStr(Pad.Layer) + ';' + FloatToStr(Pad.Rotation);
    L.Add(S);
    Pad := Iter.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(Iter);
  P := 'C:\Users\12502784\Documents\Altium_designs\IoT_SYNTHPATCH\Scripts\board_export.txt';
  L.SaveToFile(P);
  L.Free;
  ShowMessage('Exported to ' + P);
End;
