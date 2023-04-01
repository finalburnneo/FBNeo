// Copyright (c) Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "AKGamepadManager.h"
#import "AKGamepad.h"

#pragma mark - AKGamepadManager

static void gamepadWasAdded(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef device);
static void gamepadWasRemoved(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef device);

@interface AKGamepadManager ()

- (void) deviceDidConnect:(IOHIDDeviceRef) device;
- (void) deviceDidDisconnect:(IOHIDDeviceRef) device;
- (void) renumber:(BOOL) sort;

@end

@implementation AKGamepadManager
{
	IOHIDManagerRef _hidManager;
	NSMutableDictionary<NSNumber *, AKGamepad *> *_gamepadsByDeviceId;
	NSMutableArray<AKGamepad *> *_allGamepads;
	NSPointerArray *_observers;
}

+ (instancetype) sharedInstance
{
	static dispatch_once_t once;
	static id sharedInstance;
	dispatch_once(&once, ^{
		sharedInstance = [[self alloc] init];
	});
	
	return sharedInstance;
}

- (instancetype) init
{
	if ((self = [super init])) {
		_gamepadsByDeviceId = [NSMutableDictionary dictionary];
		_allGamepads = [NSMutableArray array];
        _observers = [NSPointerArray weakObjectsPointerArray];
        _hidManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
        
		NSMutableDictionary *gamepadCriterion = [@{ (NSString *) CFSTR(kIOHIDDeviceUsagePageKey): @(kHIDPage_GenericDesktop),
													(NSString *) CFSTR(kIOHIDDeviceUsageKey):  @(kHIDUsage_GD_GamePad) } mutableCopy];
		NSMutableDictionary *joystickCriterion = [@{ (NSString *) CFSTR(kIOHIDDeviceUsagePageKey): @(kHIDPage_GenericDesktop),
													(NSString *) CFSTR(kIOHIDDeviceUsageKey):  @(kHIDUsage_GD_Joystick) } mutableCopy];
		
        IOHIDManagerSetDeviceMatchingMultiple(_hidManager, (__bridge CFArrayRef) @[ gamepadCriterion, joystickCriterion ]);
        IOHIDManagerRegisterDeviceMatchingCallback(_hidManager, gamepadWasAdded, (__bridge void *) self);
        IOHIDManagerRegisterDeviceRemovalCallback(_hidManager, gamepadWasRemoved, (__bridge void *) self);
        
        IOHIDManagerScheduleWithRunLoop(_hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    }
    
    return self;
}

- (void) dealloc
{
    IOHIDManagerUnscheduleFromRunLoop(_hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    CFRelease(_hidManager);
}

- (void) deviceDidConnect:(IOHIDDeviceRef) device
{
	AKGamepad *gamepad;
	@synchronized (_gamepadsByDeviceId) {
		gamepad = [[AKGamepad alloc] initWithHidDevice:device];
		[_gamepadsByDeviceId setObject:gamepad
								forKey:@([gamepad gamepadId])];
		
		[_allGamepads addObject:gamepad];
		[self renumber:YES];
		
		[gamepad setDelegate:self];
	}
	
	[gamepad registerForEvents];
}

- (void) deviceDidDisconnect:(IOHIDDeviceRef) device
{
	@synchronized (_gamepadsByDeviceId) {
		AKGamepad *gamepad = [_gamepadsByDeviceId objectForKey:@((NSInteger) device)];
		if (gamepad) {
			[_gamepadsByDeviceId removeObjectForKey:@([gamepad gamepadId])];
			[_allGamepads removeObjectAtIndex:[gamepad index]];
			[self renumber:NO];
		}
	}
}

- (AKGamepad *) gamepadWithId:(NSInteger) gamepadId
{
    return [_gamepadsByDeviceId objectForKey:@(gamepadId)];
}

- (AKGamepad *) gamepadAtIndex:(NSUInteger) index
{
	AKGamepad *gp = nil;
	if (index < [_allGamepads count]) {
		gp = [_allGamepads objectAtIndex:index];
	}
	
	return gp;
}

- (NSUInteger) gamepadCount
{
	return [_allGamepads count];
}

- (NSArray<AKGamepad *> *) allConnected
{
	return [NSArray arrayWithArray:_allGamepads];
}

#pragma mark - AKGamepadDelegate

- (void) gamepadDidConnect:(AKGamepad *) gamepad
{
	@synchronized (_observers) {
		for (id delegate in _observers) {
			if ([delegate respondsToSelector:_cmd]) {
				[delegate gamepadDidConnect:gamepad];
			}
		}
	}
}

- (void) gamepadDidDisconnect:(AKGamepad *) gamepad
{
	@synchronized (_observers) {
		for (id delegate in _observers) {
			if ([delegate respondsToSelector:_cmd]) {
				[delegate gamepadDidDisconnect:gamepad];
			}
		}
	}
}

- (void) gamepad:(AKGamepad *) gamepad
		xChanged:(NSInteger) newValue
		  center:(NSInteger) center
	   eventData:(AKGamepadEventData *) eventData
{
	@synchronized (_observers) {
		for (id delegate in _observers) {
			if ([delegate respondsToSelector:_cmd]) {
				[delegate gamepad:gamepad
						 xChanged:newValue
						   center:center
						eventData:eventData];
			}
		}
	}
}

- (void) gamepad:(AKGamepad *) gamepad
		yChanged:(NSInteger) newValue
		  center:(NSInteger) center
	   eventData:(AKGamepadEventData *) eventData
{
	@synchronized (_observers) {
		for (id delegate in _observers) {
			if ([delegate respondsToSelector:_cmd]) {
				[delegate gamepad:gamepad
						 yChanged:newValue
						   center:center
						eventData:eventData];
			}
		}
	}
}

- (void) gamepad:(AKGamepad *) gamepad
		  button:(NSUInteger) index
		  isDown:(BOOL) isDown
	   eventData:(AKGamepadEventData *) eventData
{
	@synchronized (_observers) {
		for (id delegate in _observers) {
			if ([delegate respondsToSelector:_cmd]) {
				[delegate gamepad:gamepad
						   button:index
						   isDown:isDown
						eventData:eventData];
			}
		}
	}
}

- (void) addObserver:(id<AKGamepadEventDelegate>) observer
{
	@synchronized (_observers) {
		[_observers addPointer:(__bridge void * _Nullable)(observer)];
	}
	
#ifdef DEBUG
	NSLog(@"gamepadManager/addObserver");
#endif
}

- (void) removeObserver:(id<AKGamepadEventDelegate>) observer
{
	void *remove = (__bridge void * _Nullable)(observer);
	@synchronized (_observers) {
		for (int i = (int) [_observers count] - 1; i >= 0; i--) {
			void *ptr = [_observers pointerAtIndex:i];
			if (ptr == remove || ptr == NULL) {
				[_observers removePointerAtIndex:i];
			}
		}
	}
	
#ifdef DEBUG
	NSLog(@"gamepadManager/removeObserver");
#endif
}

#pragma mark - Private

- (void) renumber:(BOOL) sort
{
	@synchronized (_allGamepads) {
		if (sort) {
			[_allGamepads sortUsingComparator:^NSComparisonResult(AKGamepad *gp1, AKGamepad *gp2) {
				return [gp1 locationId] - [gp2 locationId];
			}];
		}
		[_allGamepads enumerateObjectsUsingBlock:^(AKGamepad *gp, NSUInteger idx, BOOL * _Nonnull stop) {
			[gp setIndex:idx];
		}];
	}
}

@end

#pragma mark - IOHID C Callbacks

void gamepadWasAdded(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef device)
{
    @autoreleasepool {
        [((__bridge AKGamepadManager *) inContext) deviceDidConnect:device];
    }
}

void gamepadWasRemoved(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef device)
{
    @autoreleasepool {
        [((__bridge AKGamepadManager *) inContext) deviceDidDisconnect:device];
    }
}
