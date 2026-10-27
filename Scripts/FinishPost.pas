{ Closes PCBServer.PreProcess calls left open by an aborted script, so the PcbDoc can be saved. }
Procedure FinishPost;
Begin
  PCBServer.PostProcess;
  PCBServer.PostProcess;
  ShowMessage('PostProcess done');
End;
