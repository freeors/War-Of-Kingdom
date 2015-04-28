/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_UIKIT

#include "SDL_video.h"
#include "SDL_assert.h"
#include "SDL_hints.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_events_c.h"

#import "SDL_uikitviewcontroller.h"
#import "SDL_uikitmessagebox.h"
#include "SDL_uikitvideo.h"
#include "SDL_uikitmodes.h"
#include "SDL_uikitwindow.h"

#if SDL_IPHONE_KEYBOARD
#include "keyinfotable.h"
int keyboardHeight2 = 0;
#endif

#import <AVFoundation/AVFoundation.h>
#import <CoreMotion/CoreMotion.h>

static const char* kRecordAudioFile = NULL;

extern SDL_bool mixable_audio;

typedef void (* fn_3axis_sensor_callback)(double x, double y, double z);
static fn_3axis_sensor_callback acceleration_callback = NULL;
static fn_3axis_sensor_callback gyroscope_callback = NULL;

SDL_bool UIBackgroundModes_audio()
{
    static int ret = -1;
    if (ret == -1) {
        NSDictionary* bundleDic = [[NSBundle mainBundle] infoDictionary];
        NSArray* background = [bundleDic objectForKey:@"UIBackgroundModes"];
        if ([background count]) {
            NSUInteger count = [background count];
            if ([background indexOfObject:@"audio"] != NSNotFound) {
                ret = 1;
            } else {
                ret = 0;
            }
        } else {
            ret = 0;
        }
    }
    return ret? SDL_TRUE: SDL_FALSE;
}

void UIKit_SetPlaybackCategory(SDL_bool mixable)
{
    SDL_bool enable_background_audio = UIBackgroundModes_audio();
    
    AVAudioSession *audioSession = [AVAudioSession sharedInstance];
    NSError* error1;
    
    // when enable_background_audio == false, should set to Ambient, but at least iOS9.0.2 work error!
    if (true /*enable_background_audio*/) {
        if (mixable) {
            [audioSession setCategory:AVAudioSessionCategoryPlayback withOptions: AVAudioSessionCategoryOptionMixWithOthers error: &error1];
        } else {
            [audioSession setCategory:AVAudioSessionCategoryPlayback error: &error1];
        }
    } else {
        if (mixable) {
            [audioSession setCategory:AVAudioSessionCategorySoloAmbient error:nil];
        } else {
            [audioSession setCategory:AVAudioSessionCategoryAmbient error: &error1];
        }
    }
    NSLog(@"setCategory with error: %@", [error1 localizedDescription]);
    [audioSession setActive:YES error: &error1];
    NSLog(@"setActive with error: %@", [error1 localizedDescription]);
}

@interface SDL_uikitviewcontroller ()<AVAudioRecorderDelegate>
@property (nonatomic, strong) AVAudioRecorder *audioRecorder;
@property (nonatomic, strong) CMMotionManager *motionManager;


@end

@implementation SDL_uikitviewcontroller {
    CADisplayLink *displayLink;
    int animationInterval;
    void (*animationCallback)(void*);
    void *animationCallbackParam;

#if SDL_IPHONE_KEYBOARD
    UITextField *textField;
#endif
}

@synthesize window;

- (instancetype)initWithSDLWindow:(SDL_Window *)_window
{
    if (self = [super initWithNibName:nil bundle:nil]) {
        self.window = _window;

#if SDL_IPHONE_KEYBOARD
        [self initKeyboard];
#endif
    }
    return self;
}

- (void)dealloc
{
#if SDL_IPHONE_KEYBOARD
    [self deinitKeyboard];
#endif
}

- (void)setAnimationCallback:(int)interval
                    callback:(void (*)(void*))callback
               callbackParam:(void*)callbackParam
{
    [self stopAnimation];

    animationInterval = interval;
    animationCallback = callback;
    animationCallbackParam = callbackParam;

    if (animationCallback) {
        [self startAnimation];
    }
}

- (void)startAnimation
{
    displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(doLoop:)];
    [displayLink setFrameInterval:animationInterval];
    [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)stopAnimation
{
    [displayLink invalidate];
    displayLink = nil;
}

- (void)doLoop:(CADisplayLink*)sender
{
    /* Don't run the game loop while a messagebox is up */
    if (!UIKit_ShowingMessageBox()) {
        animationCallback(animationCallbackParam);
    }
}

- (void)loadView
{
    /* Do nothing. */
}

- (void)viewDidLayoutSubviews
{
    const CGSize size = self.view.bounds.size;
    int w = (int) size.width;
    int h = (int) size.height;

    SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, w, h);
}

- (NSUInteger)supportedInterfaceOrientations
{
	// Fix bug temporary.
	if (window->x || window->y || window->w > 16384 || window->h > 16384 || window->min_w || window->min_h || window->max_w || window->max_h || window->hit_test || window->hit_test_data || window->prev || window->next) {
        NSUInteger orientationMask = 0;
        UIInterfaceOrientation curorient = [UIApplication sharedApplication].statusBarOrientation;
        if (UIInterfaceOrientationIsLandscape(curorient)) {
            return UIInterfaceOrientationMaskLandscapeLeft | UIInterfaceOrientationMaskLandscapeRight;
        } else {
            return UIInterfaceOrientationMaskPortrait;
        }
	}
    return UIKit_GetSupportedOrientations(window);
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orient
{
    return ([self supportedInterfaceOrientations] & (1 << orient)) != 0;
}

- (BOOL)prefersStatusBarHidden
{
    return (window->flags & (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_BORDERLESS)) != 0;
}

/*
 ---- Keyboard related functionality below this line ----
 */
#if SDL_IPHONE_KEYBOARD

@synthesize textInputRect;
@synthesize keyboardHeight;
@synthesize keyboardVisible;

static NSString* last_text_field_text = @"";

static int byte_size_from_utf8_first(unsigned char ch)
{
    int count;
    
    if ((ch & 0x80) == 0)
        count = 1;
    else if ((ch & 0xE0) == 0xC0)
        count = 2;
    else if ((ch & 0xF0) == 0xE0)
        count = 3;
    else if ((ch & 0xF8) == 0xF0)
        count = 4;
    else if ((ch & 0xFC) == 0xF8)
        count = 5;
    else if ((ch & 0xFE) == 0xFC)
        count = 6;
    else
        count = 10; // invalid characters, return dummy count.
    
    return count;
}

- (void)textFieldDidChanged:(UITextField *)_textField
{
    NSString* sub = [[_textField text] substringFromIndex:1];
    int chars = sub.length;
    int bytes = [sub lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
/*
    NSLog(@"textField text : %@, bytes: %u, %u", [_textField text], [[_textField text] length],
          [[_textField text] lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
*/
    const unsigned char * c_str = (const unsigned char*)sub.UTF8String;
    const unsigned char * last_c_str = (const unsigned char*)last_text_field_text.UTF8String;
        
    int last_bytes = [last_text_field_text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    int last_chars = last_text_field_text.length;
    int first_byte, first_chars = 0;
    int min_bytes = bytes >= last_bytes? last_bytes: bytes;
        
    for (first_byte = 0; first_byte < min_bytes; first_byte ++) {
        if (c_str[first_byte] != last_c_str[first_byte]) {
            break;
        }
    }
    // first_byte is first different byte position.
    if (first_byte == 0) {
        first_chars = 0;
            
    } else if (first_byte == min_bytes) {
        if (bytes == last_bytes) {
            return;
        }
        if (min_bytes == last_bytes) {
            first_chars = last_chars;
        } else {
            first_chars = chars;
        }
            
    } else {
        // first_byte maybe not first byte of utf-8 character.
        for (int at = 0; at <= first_byte; first_chars ++) {
            at += byte_size_from_utf8_first(last_c_str[at]);
        }
        first_chars -= 1;
    }
    // first_chars is first different character postion.
        
    int backs = last_chars - first_chars;
            
    while (backs > 0) {
        SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_BACKSPACE);
        SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_BACKSPACE);
        backs --;
    }
    SDL_SendKeyboardText([sub substringFromIndex:first_chars].UTF8String);
        
    last_text_field_text = sub;
}

/* Set ourselves up as a UITextFieldDelegate */
- (void)initKeyboard
{
    textField = [[UITextField alloc] initWithFrame:CGRectZero];
    textField.delegate = self;
    /* placeholder so there is something to delete! */
    textField.text = @" ";

    /* set UITextInputTrait properties, mostly to defaults */
    textField.autocapitalizationType = UITextAutocapitalizationTypeNone;
    textField.autocorrectionType = UITextAutocorrectionTypeNo;
    textField.enablesReturnKeyAutomatically = NO;
    textField.keyboardAppearance = UIKeyboardAppearanceDefault;
    textField.keyboardType = UIKeyboardTypeDefault;
    textField.returnKeyType = UIReturnKeyDefault;
    textField.secureTextEntry = NO;
    

    textField.hidden = YES;
    keyboardVisible = NO;

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(keyboardWillShow:) name:UIKeyboardWillShowNotification object:nil];
    [center addObserver:self selector:@selector(keyboardWillHide:) name:UIKeyboardWillHideNotification object:nil];
    [textField addTarget:self action:@selector(textFieldDidChanged:) forControlEvents:UIControlEventEditingChanged];
}

- (void)setView:(UIView *)view
{
    [super setView:view];

    [view addSubview:textField];

    if (keyboardVisible) {
        [self showKeyboard];
    }
}

- (void)deinitKeyboard
{
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center removeObserver:self name:UIKeyboardWillShowNotification object:nil];
    [center removeObserver:self name:UIKeyboardWillHideNotification object:nil];
}

/* reveal onscreen virtual keyboard */
- (void)showKeyboard
{
    keyboardVisible = YES;
    if (textField.window) {
        [textField becomeFirstResponder];
    }
}

/* hide onscreen virtual keyboard */
- (void)hideKeyboard
{
    keyboardVisible = NO;
    [textField resignFirstResponder];
    
    last_text_field_text = @"";
    [textField setText:@" "];
}

- (void)keyboardWillShow:(NSNotification *)notification
{
/*
    last_text_field_text = @"";
    if ([textField text].length != 1) {
        [textField setText:@" "];
    }
*/
    CGRect kbrect = [[notification userInfo][UIKeyboardFrameBeginUserInfoKey] CGRectValue];

    /* The keyboard rect is in the coordinate space of the screen/window, but we
     * want its height in the coordinate space of the view. */
    kbrect = [self.view convertRect:kbrect fromView:nil];

    [self setKeyboardHeight:(int)kbrect.size.height];
    {
		// special kingdom
		CGSize keyboardEndSize = [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue].size;
		keyboardHeight2 = keyboardEndSize.height;
		SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_PRINTSCREEN);
	}
}

- (void)keyboardWillHide:(NSNotification *)notification
{
    [self setKeyboardHeight:0];
}

- (void)updateKeyboard
{
    CGAffineTransform t = self.view.transform;
    CGPoint offset = CGPointMake(0.0, 0.0);
    CGRect frame = UIKit_ComputeViewFrame(window, self.view.window.screen);

    if (self.keyboardHeight) {
        int rectbottom = self.textInputRect.y + self.textInputRect.h;
        int keybottom = self.view.bounds.size.height - self.keyboardHeight;
        if (keybottom < rectbottom) {
            offset.y = keybottom - rectbottom;
        }
    }

    /* Apply this view's transform (except any translation) to the offset, in
     * order to orient it correctly relative to the frame's coordinate space. */
    t.tx = 0.0;
    t.ty = 0.0;
    offset = CGPointApplyAffineTransform(offset, t);

    /* Apply the updated offset to the view's frame. */
    frame.origin.x += offset.x;
    frame.origin.y += offset.y;

	// don't scroll application window
    // self.view.frame = frame;
}

- (void)setKeyboardHeight:(int)height
{
    keyboardVisible = height > 0;
    keyboardHeight = height;
    [self updateKeyboard];
}

/* UITextFieldDelegate method.  Invoked when user types something. */

- (BOOL)textField:(UITextField *)_textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
    
    NSUInteger len = string.length;


    if (len == 0) {
        if (textField.text.length == 1) {
            // it wants to replace text with nothing, ie a delete
            SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_BACKSPACE);
            SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_BACKSPACE);
            return NO;
        }
        
    } else {
/*
        // go through all the characters in the string we've been sent and
        // convert them to key presses
        int i;
        for (i = 0; i < len; i++) {
            unichar c = [string characterAtIndex:i];
            Uint16 mod = 0;
            SDL_Scancode code;

            if (c < 127) {
                // figure out the SDL_Scancode and SDL_keymod for this unichar
                code = unicharToUIKeyInfoTable[c].code;
                mod  = unicharToUIKeyInfoTable[c].mod;
            } else {
                // we only deal with ASCII right now
                code = SDL_SCANCODE_UNKNOWN;
                mod = 0;
            }

            if (mod & KMOD_SHIFT) {
                // If character uses shift, press shift down
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_LSHIFT);
            }

            // send a keydown and keyup even for the character
            SDL_SendKeyboardKey(SDL_PRESSED, code);
            SDL_SendKeyboardKey(SDL_RELEASED, code);

            if (mod & KMOD_SHIFT) {
                // If character uses shift, press shift back up
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_LSHIFT);
            }
        }

        SDL_SendKeyboardText([string UTF8String]);
*/
    }

    // for user interface, must add it to textfield.text
    return YES;
    
    return NO; // don't allow the edit! (keep placeholder text there)
}

/* Terminates the editing session */
- (BOOL)textFieldShouldReturn:(UITextField*)_textField
{
    SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RETURN);
    SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_RETURN);
    SDL_StopTextInput();
    return YES;
}

- (void)setReturnKeyType:(int)type
{
    textField.returnKeyType = type;
}

#endif

- (NSURL *)getSavePath
{
    NSString *urlStr = [[NSString alloc] initWithCString:(const char*)kRecordAudioFile encoding:NSASCIIStringEncoding];

    NSLog(@"file path:%@",urlStr);
    NSURL *url=[NSURL fileURLWithPath:urlStr];
    return url;
}

- (AVAudioRecorder *)audioRecorder
{
    if (!_audioRecorder) {
        NSURL *url=[self getSavePath];
        
        NSMutableDictionary *setting = [[NSMutableDictionary alloc] initWithCapacity:10];
		[setting setObject:[NSNumber numberWithInt: kAudioFormatLinearPCM] forKey: AVFormatIDKey];
        // [setting setObject:[NSNumber numberWithInt: AVAudioQualityHigh] forKey: AVEncoderAudioQualityKey];
		[setting setObject:[NSNumber numberWithFloat:8000.0] forKey: AVSampleRateKey];
        [setting setObject:[NSNumber numberWithInt:1] forKey:AVNumberOfChannelsKey];
		// Don't set AVLinearPCMBitDepthKey to 8, it will low quailty serious! for example generate noise.
		// Now iOS default Depth is 16, in order to fixed, set explict.
		[setting setObject:[NSNumber numberWithInt:16] forKey:AVLinearPCMBitDepthKey];
		[setting setObject:[NSNumber numberWithBool:NO] forKey:AVLinearPCMIsBigEndianKey];
		[setting setObject:[NSNumber numberWithBool:NO] forKey:AVLinearPCMIsFloatKey];
        
        NSError *error=nil;
        _audioRecorder=[[AVAudioRecorder alloc]initWithURL:url settings:setting error:&error];
        _audioRecorder.delegate=self;
        _audioRecorder.meteringEnabled=YES;
        if (error) {
            NSLog(@"create AVAudioRecorder %@",error.localizedDescription);
            return nil;
        }
    }
    return _audioRecorder;
}

- (void)audioRecorderDidFinishRecording:(AVAudioRecorder *)recorder successfully:(BOOL)flag
{
    NSLog(@"%@",NSStringFromSelector(_cmd));
    if ([recorder isRecording]) { 
         [recorder stop];
    }
    // [recorder release];
    _audioRecorder = nil;
    
	// Now capture's category is AVAudioSessionCategoryRecord, it is solo.
    UIKit_SetPlaybackCategory(mixable_audio);
    kRecordAudioFile = NULL;
}

- (void)audioRecorderEncodeErrorDidOccur:(AVAudioRecorder *)recorder error:(NSError *)error
{
     NSLog(@"%@",NSStringFromSelector(_cmd)); 
     // [recorder release];
} 

- (CMMotionManager *)motionManager
{
    if (!_motionManager) {
        _motionManager = [[CMMotionManager alloc] init];
    }
    return _motionManager;
}

- (void)startUpdateAccelerometer:(int)hz
{
    NSTimeInterval updateInterval = 1.0 / hz;
    
    if ([self.motionManager isAccelerometerAvailable] == YES) {
		NSLog(@"Accelerometer avaliable");
		
		if ([self.motionManager isAccelerometerActive] == YES) {
			return;
		}
		
        [self.motionManager setAccelerometerUpdateInterval:updateInterval];

        [self.motionManager startAccelerometerUpdatesToQueue:[NSOperationQueue currentQueue] withHandler:^(CMAccelerometerData *accelerometerData, NSError *error) {
			CMAcceleration acceleration = accelerometerData.acceleration;
			// as if stop, it maybe call still!
            if (acceleration_callback) {
                acceleration_callback(acceleration.x, acceleration.y, acceleration.z);
            }
        }];
    }

}

- (void)startUpdateGyroscope:(int)hz
{
    NSTimeInterval updateInterval = 1.0 / hz;
    
    if ([self.motionManager isGyroAvailable] == YES) {
		NSLog(@"Gyroscope avaliable");
		
		if ([self.motionManager isGyroActive] == YES) {
			return;
		}
		
        [self.motionManager setGyroUpdateInterval:updateInterval];

        [self.motionManager startGyroUpdatesToQueue:[NSOperationQueue currentQueue] withHandler:^(CMGyroData *gyroData, NSError *error) {
			CMRotationRate rotate = gyroData.rotationRate;
			// as if stop, it maybe call still!
            if (gyroscope_callback) {
                gyroscope_callback(rotate.x, rotate.y, rotate.z);
            }
        }];
    }

}

- (void)stopUpdateAccelerometer
{
    if ([self.motionManager isAccelerometerActive] == YES) {
        [self.motionManager stopAccelerometerUpdates];
        // [_motionManager release];
		_motionManager = nil;
    }

}

- (void)stopUpdateGyroscope
{
    if ([self.motionManager isGyroActive] == YES) {
        [self.motionManager stopGyroUpdates];
        // [_motionManager release];
		_motionManager = nil;
    }
}

@end

/* iPhone keyboard addition functions */
#if SDL_IPHONE_KEYBOARD

static SDL_uikitviewcontroller *
GetWindowViewController(SDL_Window * window)
{
    if (!window || !window->driverdata) {
        SDL_SetError("Invalid window");
        return nil;
    }

    SDL_WindowData *data = (__bridge SDL_WindowData *)window->driverdata;

    return data.viewcontroller;
}

SDL_bool
UIKit_HasScreenKeyboardSupport(_THIS)
{
    return SDL_TRUE;
}

void
UIKit_ShowScreenKeyboard(_THIS, SDL_Window *window)
{
    @autoreleasepool {
        SDL_uikitviewcontroller *vc = GetWindowViewController(window);
        [vc showKeyboard];
    }
}

void
UIKit_HideScreenKeyboard(_THIS, SDL_Window *window)
{
    @autoreleasepool {
        SDL_uikitviewcontroller *vc = GetWindowViewController(window);
        [vc hideKeyboard];
    }
}

SDL_bool
UIKit_IsScreenKeyboardShown(_THIS, SDL_Window *window)
{
    @autoreleasepool {
        SDL_uikitviewcontroller *vc = GetWindowViewController(window);
        if (vc != nil) {
            return vc.isKeyboardVisible;
        }
        return SDL_FALSE;
    }
}

void
UIKit_SetTextInputRect(_THIS, SDL_Rect *rect)
{
    if (!rect) {
        SDL_InvalidParamError("rect");
        return;
    }
    {
        rect->h = keyboardHeight2;
        return;
    }

    @autoreleasepool {
        SDL_uikitviewcontroller *vc = GetWindowViewController(SDL_GetFocusWindow());
        if (vc != nil) {
            vc.textInputRect = *rect;

            if (vc.keyboardVisible) {
                [vc updateKeyboard];
            }
        }
    }
}


#endif /* SDL_IPHONE_KEYBOARD */

extern void UIKit_PlaybackAudio(SDL_bool play);

int UIKit_MicrophonePermission()
{
    __block int ret = 0;
    __block SDL_bool executed = SDL_FALSE;
    
    AVAudioSession* audioSession = [AVAudioSession sharedInstance];
    
    if ([audioSession respondsToSelector:@selector(requestRecordPermission:)]) {
        [audioSession performSelector:@selector(requestRecordPermission:) withObject:^(BOOL granted) {
            if (granted) {
                // Microphone enabled code
                ret = 1;
            } else {
                // Microphone disabled code
                ret = 0;
            }
            executed = SDL_TRUE;
        }];
    }
 
    return executed? ret: -1;
}

void UIKit_StartRecordAudio(SDL_Window* window, const char* file)
{
	AVAudioSession* audioSession = [AVAudioSession sharedInstance]; 
	
	NSError* error1;
	if (mixable_audio) {
        // UIKit_PlaybackAudio(SDL_FALSE);
		[audioSession setCategory:AVAudioSessionCategoryPlayAndRecord withOptions: AVAudioSessionCategoryOptionMixWithOthers error: &error1];
	} else {
		[audioSession setCategory:AVAudioSessionCategoryRecord error: &error1];
	}
    
    NSLog(@"UIKit_StartRecord, setCategory with error: %@", [error1 localizedDescription]);
    
	// active audio session.
	[audioSession setActive:YES error: &error1];
    NSLog(@"UIKit_StartRecord, setActive with error: %@", [error1 localizedDescription]);
    
    NSLog(@"record...");
    
    SDL_uikitviewcontroller *vc = GetWindowViewController(window);
    
    kRecordAudioFile = file;
    if (![vc.audioRecorder isRecording]) {
        [vc.audioRecorder record];
    }
    if (mixable_audio) {
        UIKit_PlaybackAudio(SDL_FALSE);
    }
	NSLog(@"record begin...");
}

void UIKit_StopRecordAudio(SDL_Window* window)
{
	if (!kRecordAudioFile) {
	 	return;
	}
	
	SDL_uikitviewcontroller *vc = GetWindowViewController(window);

	[vc.audioRecorder stop];
	NSLog(@"record stop...");
}

void UIKit_StartAccelerometer(SDL_Window* window, const int hz, fn_3axis_sensor_callback fn, SDL_bool gyroscope)
{
	SDL_uikitviewcontroller *vc = GetWindowViewController(window);
	if (gyroscope) {
		gyroscope_callback = fn;
		
		[vc startUpdateGyroscope:hz];
		NSLog(@"gyroscope begin...");
	} else {
		acceleration_callback = fn;
		
		[vc startUpdateAccelerometer:hz];
		NSLog(@"accelerometer begin...");
	}
}

void UIKit_StopAccelerometer(SDL_Window* window, SDL_bool gyroscope)
{
	SDL_uikitviewcontroller *vc = GetWindowViewController(window);
	if (gyroscope) {
		[vc stopUpdateGyroscope];
		NSLog(@"gyroscope stop...");
		
		gyroscope_callback = NULL;
	} else {
		[vc stopUpdateAccelerometer];
		NSLog(@"accelerometer stop...");
		
		acceleration_callback = NULL;
	}
}

int UIKit_GetScale()
{
	CGFloat scale = 1.0;
#ifdef __IPHONE_8_0
	if ([[UIScreen mainScreen] respondsToSelector:@selector(nativeScale)]) {
		scale = [UIScreen mainScreen].nativeScale;
	} else
#endif
	{
		scale = [UIScreen mainScreen].scale;
	}
	return (int)scale;
}

void UIKit_ShowStatusBar(SDL_bool show)
{
	if (show) {
    	[[UIApplication sharedApplication] setStatusBarHidden:NO withAnimation:NO];
    } else {
    	[[UIApplication sharedApplication] setStatusBarHidden:YES withAnimation:NO];
    }
}

void UIKit_SetStatusBarStyle(SDL_bool light)
{
	if (light) {
    	[[UIApplication sharedApplication] setStatusBarStyle:UIStatusBarStyleLightContent];
    } else {
    	[[UIApplication sharedApplication] setStatusBarStyle:UIStatusBarStyleDefault];
    }
}

void UIKit_IExplore(const char* url)
{
    NSString *urlStr = [[NSString alloc] initWithCString:url encoding:NSASCIIStringEncoding];
	[[UIApplication sharedApplication]openURL:[NSURL URLWithString:urlStr]];
}

void UIKit_EnableBatteryMonitoring(SDL_bool enable)
{
	UIDevice* device = [UIDevice currentDevice];
	device.batteryMonitoringEnabled = enable? YES: NO;
}

int UIKit_GetBatteryLevel()
{
	UIDevice* device = [UIDevice currentDevice];
	return (int)(device.batteryLevel * 100);
}

#define SDL_IPHONE_WX 1

#if SDL_IPHONE_WX
#import "WXApiObject.h"
#import "WXApi.h"

void wechat_registerapp()
{
    NSDictionary* bundleDic = [[NSBundle mainBundle] infoDictionary];
    NSArray* urls = [bundleDic objectForKey:@"CFBundleURLTypes"];
    if (![urls count]) {
        return;
    }
    NSUInteger count = [urls count];
    for (int i = 0; i < count; i++) {
        NSDictionary* url = [urls objectAtIndex: i];
        
        if (![url count]) {
            continue;
        }
        NSString* name = [url objectForKey:@"CFBundleURLName"];
        NSArray* schemes = [url objectForKey:@"CFBundleURLSchemes"];
        
        if ([name isEqualToString: @"wechat"] && [schemes count] == 1) {
            NSString* scheme = [schemes objectAtIndex: 0];
            [WXApi registerApp: scheme];
            return;
        }
        
    }
    return;
}

/*
@interface SendMSGtoWeChat : NSObject

-(void)sendTextContent;


- (void) sendImageContent;


@end

@implementation SendMSGtoWeChat

- (void) sendTextContent
{
    SendMessageToWXReq* req = [[SendMessageToWXReq alloc] init];
    req.text = @"test thermometer";
    req.bText = YES;
    req.scene = WXSceneTimeline;
    
    [WXApi sendReq:req];
}


- (void) sendImageContent
{
    WXMediaMessage *message = [WXMediaMessage message];
    [message setThumbImage:[UIImage imageNamed:@"ScreenShot.png"]];
    
    WXImageObject *ext = [WXImageObject object];
    NSString *filePath = [[NSBundle mainBundle] pathForResource:@"ScreenShot" ofType:@"png"];
    NSLog(@"filepath :%@",filePath);
    ext.imageData = [NSData dataWithContentsOfFile:filePath];
    
    //UIImage* image = [UIImage imageWithContentsOfFile:filePath];
    UIImage* image = [UIImage imageWithData:ext.imageData];
    ext.imageData = UIImagePNGRepresentation(image);
    
    //    UIImage* image = [UIImage imageNamed:@"res5thumb.png"];
    //    ext.imageData = UIImagePNGRepresentation(image);
    
    message.mediaObject = ext;
    
    SendMessageToWXReq* req = [[SendMessageToWXReq alloc] init];
    req.bText = NO;
    req.message = message;
    req.scene = WXSceneTimeline;
    
    [WXApi sendReq:req];
}

@end
*/

void sendTextContent()
{
    SendMessageToWXReq* req = [[SendMessageToWXReq alloc] init];
    req.text = @"Thermometer test";
    req.bText = YES;
    req.scene = WXSceneTimeline;
    
    [WXApi sendReq:req];
}


void wx_send_file_png(const char* file)
{
    WXMediaMessage *message = [WXMediaMessage message];
    [message setThumbImage:[UIImage imageNamed:@"ScreenShot.png"]];
    
    WXImageObject *ext = [WXImageObject object];
    NSString *filePath = [[NSBundle mainBundle] pathForResource:@"ScreenShot" ofType:@"png"];
    NSLog(@"filepath :%@",filePath);
    ext.imageData = [NSData dataWithContentsOfFile:filePath];
    
    //UIImage* image = [UIImage imageWithContentsOfFile:filePath];
    UIImage* image = [UIImage imageWithData:ext.imageData];
    ext.imageData = UIImagePNGRepresentation(image);
    
    //    UIImage* image = [UIImage imageNamed:@"res5thumb.png"];
    //    ext.imageData = UIImagePNGRepresentation(image);
    
    message.mediaObject = ext;
    
    SendMessageToWXReq* req = [[SendMessageToWXReq alloc] init];
    req.bText = NO;
    req.message = message;
    req.scene = WXSceneTimeline;
    
    [WXApi sendReq:req];
}

void wx_send_mem_png(SDL_bool timeline, const unsigned char* data, int size)
{
	WXMediaMessage *message = [WXMediaMessage message];
    // [message setThumbImage:[UIImage imageNamed:@"ScreenShot.png"]];
    
	NSData* data2 = [NSData dataWithBytes:data length:size];
    WXImageObject* ext = [WXImageObject object];
    ext.imageData = [NSData dataWithBytes:data length:size];
	ext.imageData = data2;
    message.mediaObject = ext;
    
    SendMessageToWXReq* req = [[SendMessageToWXReq alloc] init];
    req.bText = NO;
    req.message = message;
    req.scene = timeline? WXSceneTimeline: WXSceneSession;
    
    [WXApi sendReq:req];
}

void UIKit_wechat(SDL_bool timeline, const char* text, const unsigned char* data, int size)
{
	wx_send_mem_png(timeline, data, size);
}

#else
void UIKit_wechat(SDL_bool timeline, const char* text, const unsigned char* data, int size) {}
#endif

int UIKit_AppInstalled(const char* app)
{
	int installed = 0;
	if (!SDL_memcmp(app, "wx", 2)) {
#if SDL_IPHONE_WX
		if ([WXApi isWXAppInstalled]) {
			installed = 1;
		}
#endif
	}
	return installed;
}

enum {returnkey_default, 
	returnkey_done};

void UIKit_SetKeyboardReturn(SDL_Window* window, int type)
{
	SDL_uikitviewcontroller* vc = GetWindowViewController(window);

	if (type == returnkey_done) {
		type = UIReturnKeyDone;
	} else {
		type = UIReturnKeyDefault;
	}
#if SDL_IPHONE_KEYBOARD
	[vc setReturnKeyType:type];
#endif
}

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
