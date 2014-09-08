bcc makeobj.c
makeobj c AUDIODCT.KDR ..\kdradict.obj _audiodict
makeobj f AUDIOHHD.KDR ..\kdrahead.obj _AudioHeader _audiohead
makeobj c CGADICT.KDR ..\kdrcdict.obj _CGAdict
makeobj f CGAHEAD.KDR ..\kdrchead.obj CGA_grafixheader _CGAhead
makeobj f CONTEXT.KDR ..\context.obj
makeobj c EGADICT.KDR ..\kdredict.obj _EGAdict
makeobj f EGAHEAD.KDR ..\kdrehead.obj EGA_grafixheader _EGAhead
makeobj f GAMETEXT.KDR ..\gametext.obj
makeobj c MAPDICT.KDR ..\kdrmdict.obj _mapdict
makeobj f MAPHEAD.KDR ..\kdrmhead.obj MapHeader _maphead
makeobj s PIRACY.SCN ..\piracy.h 7
makeobj f STORY.KDR ..\story.obj
