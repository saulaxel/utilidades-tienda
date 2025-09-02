unit main;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs, StdCtrls;

type

  { TForm1 }

  TForm1 = class(TForm)
    ButtonCalculate: TButton;
    TextLonePage: TEdit;
    LabelProgress: TLabel;
    LabelLonePage: TLabel;
    LabelFront: TLabel;
    LabelBack: TLabel;
    MemoFront: TMemo;
    MemoBack: TMemo;
    TextStartPage: TEdit;
    TextEndPage: TEdit;
    LabelStartPage: TLabel;
    LabelEndPage: TLabel;
    procedure ButtonCalculateClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
  private

  public

  end;

var
  Form1: TForm1;

implementation

{$R *.lfm}

{ TForm1 }
var
  indicator: array [0..3] of String;
  times_calculated: longint;

function IsEven(val: longint): Boolean;
begin
  Result := (val mod 2 = 0);
end;

function NumberOfPagesInInterval(start_page: longint; end_page: longint): longint;
begin
  Result := end_page - start_page + 1;
end;

function AscendingSequence(start_range: longint; end_range: longint): String;
var
  i: longint;
begin
  Result := IntToStr(start_range);
  i := start_range + 2;
  while i <= end_range do
  begin
    Result := Result + ',';
    Result := Result + IntToStr(i);
    i := i + 2;
  end;
end;

function DescendingSequence(start_range: longint; end_range: longint): String;
var
  i: longint;
begin
  Result := IntToStr(start_range);
  i := start_range - 2;
  while i >= end_range do
  begin
    Result := Result + ',';
    Result := Result + IntToStr(i);
    i := i - 2;
  end;
end;

procedure TForm1.FormCreate(Sender: TObject);
begin
  indicator[0] := '\\\\\ \\\\\ \\\\\ \\\\\ \\\\\ \\\\\ \\\\\ \\\\\ \\\\\ \\\\\ \\\\\ \\\\\ \\\\\ \\\\\';
  indicator[1] := '||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| ||||| |||||';
  indicator[2] := '///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////';
  indicator[3] := '----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----';
  times_calculated := 0;
end;

procedure TForm1.ButtonCalculateClick(Sender: TObject);
var
  start_page, end_page: longint;
  front_pages, back_pages, lone_page: String;
begin
  lone_page := '';
  times_calculated := times_calculated + 1;

  if (not TryStrToInt(TextStartPage.Text, start_page)) or (start_page <= 0) then
  begin
    MessageDlg('Error', 'Página inicial inválida', mtError, [mbOk], 0);
    Exit;
  end;

  if (not TryStrToInt(TextEndPage.Text, end_page)) or (end_page <= 0) then
  begin
    MessageDlg('Error', 'Página final inválida', mtError, [mbOk], 0);
    Exit;
  end;

  if start_page >= end_page then
  begin
    MessageDlg('Error', 'Página inicial debe ser menor a página final', mtError, [mbOk], 0);
    Exit;
  end;

  if IsEven(NumberOfPagesInInterval(start_page, end_page)) then
  begin
    front_pages := AscendingSequence(start_page, end_page - 1);
    back_pages := DescendingSequence(end_page, start_page);
  end
  else
  begin
    front_pages := AscendingSequence(start_page, end_page - 2);
    back_pages := DescendingSequence(end_page - 1, start_page);
    lone_page := IntToStr(end_page);
    MessageDlg('Aviso', 'La última página no tendrá reverso. Imprimirla por separado más tarde.', mtInformation, [mbOk], 0);
  end;

  MemoFront.Lines.Clear();
  MemoFront.Lines.Add(front_pages);
  MemoBack.Lines.Clear();
  MemoBack.Lines.Add(back_pages);
  TextLonePage.Text := lone_page;
  LabelProgress.Caption := indicator[times_calculated mod Length(indicator)];
end;

end.

