#import <Cocoa/Cocoa.h>
#import <PDFKit/PDFKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface ScanApp : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property NSWindow *window;
@property NSTextField *statusLabel, *destinationLabel;
@property NSButton *startButton, *cancelButton, *openButton, *folderButton, *logButton;
@property NSPopUpButton *dpiChoice, *modeChoice;
@property NSButton *paperCleanup, *edgeCleanup;
@property NSProgressIndicator *progress;
@property NSTask *task;
@property NSURL *destination, *job, *resultURL, *logURL;
@property NSInteger pages;
@property BOOL cancelling, terminating;
- (void)buildWindow;
@end

@implementation ScanApp
- (NSTextField *)label:(NSString *)text frame:(NSRect)frame size:(CGFloat)size {
    NSTextField *label=[NSTextField wrappingLabelWithString:text];
    label.frame=frame; label.font=[NSFont systemFontOfSize:size];
    [self.window.contentView addSubview:label]; return label;
}
- (NSButton *)button:(NSString *)title frame:(NSRect)frame action:(SEL)action {
    NSButton *b=[NSButton buttonWithTitle:title target:self action:action];
    b.frame=frame; b.bezelStyle=NSBezelStyleRounded;
    [self.window.contentView addSubview:b]; return b;
}
- (void)buildWindow {
    self.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,560,470)
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable
        backing:NSBackingStoreBuffered defer:NO];
    self.window.title=@"Alaris macOS Scan Tool"; self.window.delegate=self;
    self.window.releasedWhenClosed=NO;
    NSTextField *title=[self label:@"Alaris macOS Scan Tool" frame:NSMakeRect(24,416,512,32) size:25];
    title.font=[NSFont systemFontOfSize:25 weight:NSFontWeightSemibold];
    NSTextField *subtitle=[self label:@"Alaris S2050 · A4 · Vorderseite · PDF" frame:NSMakeRect(24,389,512,22) size:13];
    subtitle.textColor=NSColor.secondaryLabelColor;
    [self label:@"Blätter einlegen und Scan starten. Alle Seiten werden zu einem PDF zusammengefasst. Der Scan endet bei leerem Einzug." frame:NSMakeRect(24,331,512,48) size:14];
    [self label:@"Auflösung" frame:NSMakeRect(24,292,80,22) size:13];
    self.dpiChoice=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(105,288,116,28) pullsDown:NO];
    [self.dpiChoice addItemsWithTitles:@[@"150 dpi",@"300 dpi",@"600 dpi"]];
    for (NSInteger i=0;i<3;i++) [self.dpiChoice itemAtIndex:i].tag=[@[@150,@300,@600][i] integerValue];
    [self.window.contentView addSubview:self.dpiChoice];
    [self label:@"Farbmodus" frame:NSMakeRect(242,292,80,22) size:13];
    self.modeChoice=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(323,288,213,28) pullsDown:NO];
    [self.modeChoice addItemsWithTitles:@[@"Graustufen",@"Farbe",@"Schwarzweiß"]];
    for (NSInteger i=0;i<3;i++) [self.modeChoice itemAtIndex:i].representedObject=@[@"gray",@"color",@"bw"][i];
    [self.window.contentView addSubview:self.modeChoice];
    self.paperCleanup=[NSButton checkboxWithTitle:@"Papierhintergrund aufhellen" target:nil action:NULL];
    self.paperCleanup.frame=NSMakeRect(24,249,512,24);self.paperCleanup.state=NSControlStateValueOn;
    self.paperCleanup.toolTip=@"Neutralen Papierhintergrund auf Weiß setzen. Für Fotos oder bewusst graue Flächen ausschalten.";
    [self.window.contentView addSubview:self.paperCleanup];
    self.edgeCleanup=[NSButton checkboxWithTitle:@"Schwarzen Scanrand entfernen" target:nil action:NULL];
    self.edgeCleanup.frame=NSMakeRect(24,220,512,24);self.edgeCleanup.state=NSControlStateValueOn;
    self.edgeCleanup.toolTip=@"Schmale dunkle Streifen direkt am Blattrand auf Weiß setzen. Für randfüllende Vorlagen ausschalten.";
    [self.window.contentView addSubview:self.edgeCleanup];
    self.destinationLabel=[self label:@"Speicherort und Dateiname werden beim Start gewählt." frame:NSMakeRect(24,177,512,24) size:12];
    self.destinationLabel.textColor=NSColor.secondaryLabelColor;
    self.destinationLabel.maximumNumberOfLines=1;
    self.destinationLabel.lineBreakMode=NSLineBreakByTruncatingMiddle;
    self.statusLabel=[self label:@"Bereit · maximal 100 Seiten pro Vorgang" frame:NSMakeRect(24,137,512,32) size:14];
    self.progress=[[NSProgressIndicator alloc] initWithFrame:NSMakeRect(24,118,512,8)];
    self.progress.style=NSProgressIndicatorStyleBar; self.progress.indeterminate=YES;
    self.progress.hidden=YES; [self.window.contentView addSubview:self.progress];
    self.startButton=[self button:@"Scan starten" frame:NSMakeRect(18,65,318,38) action:@selector(start:)];
    self.startButton.keyEquivalent=@"\r";
    self.cancelButton=[self button:@"Abbrechen" frame:NSMakeRect(344,65,198,38) action:@selector(cancel:)];
    self.cancelButton.enabled=NO;
    self.openButton=[self button:@"PDF öffnen" frame:NSMakeRect(18,20,150,30) action:@selector(openPDF:)];
    self.folderButton=[self button:@"Ordner anzeigen" frame:NSMakeRect(174,20,178,30) action:@selector(showFolder:)];
    self.logButton=[self button:@"Protokoll öffnen" frame:NSMakeRect(358,20,184,30) action:@selector(openLog:)];
    self.openButton.enabled=self.folderButton.enabled=self.logButton.enabled=NO;
    [self.window center];
}
- (void)applicationDidFinishLaunching:(NSNotification *)note {
    (void)note; [self buildWindow];
    NSMenu *menu=[NSMenu new]; NSMenuItem *appItem=[NSMenuItem new];
    [menu addItem:appItem]; NSMenu *appMenu=[NSMenu new];
    [appMenu addItemWithTitle:@"S2050 Scan beenden" action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu=appMenu; NSApp.mainMenu=menu;
    [self.window makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
}
- (void)start:(id)sender {
    (void)sender; if (self.task) return;
    NSSavePanel *panel=[NSSavePanel savePanel];
    panel.title=@"Scan als PDF speichern"; panel.allowedContentTypes=@[UTTypePDF];
    panel.canCreateDirectories=YES;
    NSDateFormatter *date=[NSDateFormatter new]; date.dateFormat=@"yyyy-MM-dd_HH-mm-ss";
    panel.nameFieldStringValue=[NSString stringWithFormat:@"Scan_%@.pdf",[date stringFromDate:NSDate.date]];
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response==NSModalResponseOK) { self.destination=panel.URL; [self runScan]; }
    }];
}
- (void)runScan {
    NSFileManager *fm=NSFileManager.defaultManager; NSError *error=nil;
    NSURL *support=[fm URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:&error];
    if (!support) { self.statusLabel.stringValue=@"Arbeitsordner konnte nicht angelegt werden."; return; }
    self.job=[[[support URLByAppendingPathComponent:@"S2050 Scan/Scans" isDirectory:YES]
        URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES] standardizedURL];
    if (![fm createDirectoryAtURL:self.job withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:&error]) {
        self.statusLabel.stringValue=@"Arbeitsordner konnte nicht angelegt werden."; return;
    }
    self.logURL=[self.job URLByAppendingPathComponent:@"scan.log"];
    if (![fm createFileAtPath:self.logURL.path contents:nil attributes:@{NSFilePosixPermissions:@0600}]) {
        self.statusLabel.stringValue=@"Protokoll konnte nicht angelegt werden."; return;
    }
    NSFileHandle *log=[NSFileHandle fileHandleForWritingToURL:self.logURL error:&error];
    if (!log) { self.statusLabel.stringValue=@"Protokoll konnte nicht geöffnet werden."; return; }
    NSTask *task=[NSTask new];
    task.executableURL=[NSBundle.mainBundle.bundleURL URLByAppendingPathComponent:@"Contents/MacOS/s2050-probe"];
    task.arguments=@[@"--scan-stack",[[self.job URLByAppendingPathComponent:@"pages" isDirectory:YES] path],
        @"--dpi",[NSString stringWithFormat:@"%ld",(long)self.dpiChoice.selectedItem.tag],
        @"--mode",self.modeChoice.selectedItem.representedObject,
        @"--paper-cleanup",self.paperCleanup.state==NSControlStateValueOn?@"on":@"off",
        @"--edge-cleanup",self.edgeCleanup.state==NSControlStateValueOn?@"on":@"off"];
    NSPipe *pipe=[NSPipe pipe]; task.standardOutput=pipe; task.standardError=pipe;
    self.task=task; self.pages=0; self.cancelling=NO; self.resultURL=nil;
    self.startButton.enabled=NO; self.cancelButton.enabled=YES;
    self.dpiChoice.enabled=self.modeChoice.enabled=NO;
    self.paperCleanup.enabled=self.edgeCleanup.enabled=NO;
    self.openButton.enabled=NO; self.folderButton.enabled=self.logButton.enabled=YES;
    self.destinationLabel.stringValue=self.destination.path;
    self.statusLabel.stringValue=@"Scanner wird verbunden …";
    self.progress.hidden=NO; [self.progress startAnimation:nil];
    if (![task launchAndReturnError:&error]) {
        NSData *message=[[NSString stringWithFormat:@"Startfehler: %@\n",error.localizedDescription] dataUsingEncoding:NSUTF8StringEncoding];
        [log writeData:message]; [log closeFile]; [self finished:1]; return;
    }
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        NSMutableData *pending=[NSMutableData data];
        @try {
            while (YES) {
                NSData *data=[pipe.fileHandleForReading readDataOfLength:4096]; if (!data.length) break;
                [log writeData:data]; [pending appendData:data];
                NSRange newline;
                while ((newline=[pending rangeOfData:[NSData dataWithBytes:"\n" length:1] options:0 range:NSMakeRange(0,pending.length)]).location!=NSNotFound) {
                    NSData *lineData=[pending subdataWithRange:NSMakeRange(0,newline.location)];
                    NSString *line=[[NSString alloc] initWithData:lineData encoding:NSUTF8StringEncoding];
                    [pending replaceBytesInRange:NSMakeRange(0,newline.location+1) withBytes:NULL length:0];
                    if (line) dispatch_async(dispatch_get_main_queue(),^{ [self consume:line]; });
                }
            }
        } @catch (NSException *exception) {
            (void)exception; if (task.running) [task terminate];
        }
        [pipe.fileHandleForReading closeFile]; [task waitUntilExit];
        @try { [log synchronizeFile]; [log closeFile]; } @catch (NSException *exception) { (void)exception; }
        int code=task.terminationStatus;
        dispatch_async(dispatch_get_main_queue(),^{ [self finished:code]; });
    });
}
- (void)consume:(NSString *)line {
    if ([line hasPrefix:@"PAGE_SAVED "]) {
        self.pages=[[line substringFromIndex:11] integerValue];
        if (!self.cancelling) self.statusLabel.stringValue=[NSString stringWithFormat:@"%ld Seite%@ gesichert · Einzug wird geprüft …",(long)self.pages,self.pages==1?@"":@"n"];
    } else if ([line hasPrefix:@"PAGE_START "] && !self.cancelling) {
        self.statusLabel.stringValue=[NSString stringWithFormat:@"Seite %ld wird gescannt …",(long)[[line substringFromIndex:11] integerValue]];
    }
}
- (BOOL)publish:(NSURL *)source error:(NSError **)error {
    NSFileManager *fm=NSFileManager.defaultManager;
    NSURL *temporary=[[self.destination URLByDeletingLastPathComponent] URLByAppendingPathComponent:[NSString stringWithFormat:@".s2050-%@.pdf",NSUUID.UUID.UUIDString]];
    if (![fm copyItemAtURL:source toURL:temporary error:error]) return NO;
    BOOL ok;
    if ([fm fileExistsAtPath:self.destination.path])
        ok=[fm replaceItemAtURL:self.destination withItemAtURL:temporary backupItemName:nil options:0 resultingItemURL:NULL error:error];
    else ok=[fm moveItemAtURL:temporary toURL:self.destination error:error];
    if (!ok) [fm removeItemAtURL:temporary error:NULL];
    return ok;
}
- (void)finished:(int)code {
    self.task=nil; self.cancelButton.enabled=NO; self.startButton.enabled=YES;
    self.dpiChoice.enabled=self.modeChoice.enabled=YES;
    self.paperCleanup.enabled=self.edgeCleanup.enabled=YES;
    [self.progress stopAnimation:nil]; self.progress.hidden=YES;
    NSURL *pages=[self.job URLByAppendingPathComponent:@"pages" isDirectory:YES];
    NSURL *pdf=[pages URLByAppendingPathComponent:code==0?@"scan.pdf":@"scan-partial.pdf"];
    PDFDocument *document=[[PDFDocument alloc] initWithURL:pdf];
    /* Cancellation can arrive while an already complete PDF is being written.
     * Keep that document accessible, but do not report a cancelled job as success. */
    if (code!=0 && !document.pageCount) {
        pdf=[pages URLByAppendingPathComponent:@"scan.pdf"];
        document=[[PDFDocument alloc] initWithURL:pdf];
    }
    if (document.pageCount>0) {
        self.resultURL=pdf; self.pages=document.pageCount; self.openButton.enabled=YES;
        if (code==0) {
            NSError *error=nil;
            if ([self publish:pdf error:&error]) {
                self.resultURL=self.destination;
                self.statusLabel.stringValue=[NSString stringWithFormat:@"%ld Seite%@ als PDF gespeichert.",(long)self.pages,self.pages==1?@"":@"n"];
            } else {
                self.statusLabel.stringValue=@"PDF erstellt; Speichern fehlgeschlagen. Arbeitskopie erhalten.";
                self.destinationLabel.stringValue=error.localizedDescription;
            }
        } else self.statusLabel.stringValue=[NSString stringWithFormat:@"%@ · %ld Seite%@ im Teil-PDF gesichert.",self.cancelling?@"Abgebrochen":@"Scanfehler",(long)self.pages,self.pages==1?@"":@"n"];
    } else {
        self.statusLabel.stringValue=self.cancelling?@"Abgebrochen. Noch kein vollständiges PDF vorhanden.":@"Kein PDF erstellt. Papier, USB und Protokoll prüfen.";
    }
    if (self.terminating) [NSApp replyToApplicationShouldTerminate:YES];
}
- (void)cancel:(id)sender {
    (void)sender; if (!self.task || self.cancelling) return;
    self.cancelling=YES; self.cancelButton.enabled=NO;
    self.statusLabel.stringValue=@"Wird beendet · fertige Seiten werden gesichert …";
    if (self.task.running) [self.task terminate];
}
- (void)openPDF:(id)sender { (void)sender; if (self.resultURL) [NSWorkspace.sharedWorkspace openURL:self.resultURL]; }
- (void)showFolder:(id)sender { (void)sender; NSURL *url=self.resultURL?:self.job; if (url) [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[url]]; }
- (void)openLog:(id)sender { (void)sender; if (self.logURL) [NSWorkspace.sharedWorkspace openURL:self.logURL]; }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    (void)sender; if (!self.task) return NSTerminateNow;
    self.terminating=YES; [self cancel:nil]; return NSTerminateLater;
}
- (BOOL)windowShouldClose:(NSWindow *)sender {
    (void)sender; if (self.task) { [NSApp terminate:nil]; return NO; } return YES;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { (void)sender; return YES; }
@end

int main(int argc,const char **argv) {
    @autoreleasepool {
        if (argc==2 && !strcmp(argv[1],"--self-test")) {
            /* Headless tests for the bundled backend and atomic destination save. */
            NSFileManager *fm=NSFileManager.defaultManager;
            NSURL *root=[NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString] isDirectory:YES];
            NSError *error=nil;
            if (![fm createDirectoryAtURL:root withIntermediateDirectories:YES attributes:nil error:&error]) return 1;
            NSURL *source=[root URLByAppendingPathComponent:@"source.pdf"];
            NSData *bytes=[@"synthetic PDF bytes" dataUsingEncoding:NSUTF8StringEncoding];
            if (![bytes writeToURL:source options:NSDataWritingAtomic error:&error]) return 1;
            ScanApp *test=[ScanApp new]; test.destination=[root URLByAppendingPathComponent:@"destination.pdf"];
            if (![test publish:source error:&error] || ![[NSData dataWithContentsOfURL:test.destination] isEqual:bytes]) return 1;
            NSData *replacement=[@"second synthetic PDF" dataUsingEncoding:NSUTF8StringEncoding];
            if (![replacement writeToURL:source options:NSDataWritingAtomic error:&error] ||
                ![test publish:source error:&error] || ![[NSData dataWithContentsOfURL:test.destination] isEqual:replacement]) return 1;
            if ([test publish:[root URLByAppendingPathComponent:@"missing.pdf"] error:&error] ||
                ![[NSData dataWithContentsOfURL:test.destination] isEqual:replacement]) return 1;
            [fm removeItemAtURL:root error:NULL];
            NSTask *task=[NSTask new]; task.executableURL=[NSBundle.mainBundle.bundleURL URLByAppendingPathComponent:@"Contents/MacOS/s2050-probe"];
            task.arguments=@[@"--help"]; NSPipe *pipe=[NSPipe pipe];task.standardOutput=pipe;task.standardError=pipe;
            if (![task launchAndReturnError:&error]) return 1;
            NSData *data=[pipe.fileHandleForReading readDataToEndOfFile];[task waitUntilExit];
            NSString *help=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            if (task.terminationStatus || ![help containsString:@"--scan-stack"]) return 1;
            puts("App self-test passed: bundled backend, new PDF save, replacement, failed-save preservation.");return 0;
        }
        NSApplication *app=NSApplication.sharedApplication;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        ScanApp *delegate=[ScanApp new]; app.delegate=delegate;
        if (argc==3 && !strcmp(argv[1],"--preview")) {
            [delegate buildWindow];
            NSView *view=delegate.window.contentView;
            NSBitmapImageRep *bitmap=[view bitmapImageRepForCachingDisplayInRect:view.bounds];
            [view cacheDisplayInRect:view.bounds toBitmapImageRep:bitmap];
            NSData *png=[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
            return [png writeToFile:[NSString stringWithUTF8String:argv[2]] atomically:YES]?0:1;
        }
        [app run];
    }
    return 0;
}
