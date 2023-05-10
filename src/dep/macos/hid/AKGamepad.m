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

#import "AKGamepad.h"

#import "AKGamepadManager.h"

#import <IOKit/hid/IOHIDLib.h>

#pragma mark - AKGamepad

static void gamepadInputValueCallback(void *context, IOReturn result, void *sender, IOHIDValueRef value);

@interface AKGamepad()

- (void) unregisterFromEvents;
- (void) didReceiveInputValue:(IOHIDValueRef) valueRef;

@end

@implementation AKGamepad
{
	BOOL registeredForEvents;
	IOHIDDeviceRef hidDevice;
	
	NSPoint _axes;
}

- (id) initWithHidDevice:(IOHIDDeviceRef) device
{
    if ((self = [self init])) {
        registeredForEvents = NO;
        hidDevice = device;
        
        CFRetain(hidDevice);
        
        _vendorId = 0;
        _productId = 0;
		_gamepadId = (NSInteger) device;
        _axes = NSMakePoint(CGFLOAT_MIN, CGFLOAT_MIN);
        
        CFTypeRef tCFTypeRef;
        CFTypeID numericTypeId = CFNumberGetTypeID();
        
        tCFTypeRef = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDVendorIDKey));
		if (tCFTypeRef && CFGetTypeID(tCFTypeRef) == numericTypeId) {
            CFNumberGetValue((CFNumberRef)tCFTypeRef, kCFNumberSInt32Type, &_vendorId);
		}
		
        tCFTypeRef = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDProductIDKey));
		if (tCFTypeRef && CFGetTypeID(tCFTypeRef) == numericTypeId) {
            CFNumberGetValue((CFNumberRef)tCFTypeRef, kCFNumberSInt32Type, &_productId);
		}
		
        tCFTypeRef = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDLocationIDKey));
		if (tCFTypeRef && CFGetTypeID(tCFTypeRef) == numericTypeId) {
            CFNumberGetValue((CFNumberRef)tCFTypeRef, kCFNumberSInt32Type, &_locationId);
		}
		
        _name = (__bridge NSString *)IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDProductKey));
    }
    
    return self;
}

- (void) dealloc
{
    [self unregisterFromEvents];
    
    CFRelease(hidDevice);
}

- (void) registerForEvents
{
    if (!registeredForEvents) {
        IOHIDDeviceOpen(hidDevice, kIOHIDOptionsTypeNone);
        IOHIDDeviceRegisterInputValueCallback(hidDevice, gamepadInputValueCallback, (__bridge void *)(self));
        
        registeredForEvents = YES;
        
		if ([_delegate respondsToSelector:@selector(gamepadDidConnect:)]) {
            [_delegate gamepadDidConnect:self];
		}
    }
}

- (void) unregisterFromEvents
{
    if (registeredForEvents) {
        // FIXME: why does this crash the emulator?
//        IOHIDDeviceClose(hidDevice, kIOHIDOptionsTypeNone);
        
        registeredForEvents = NO;
        
		if ([_delegate respondsToSelector:@selector(gamepadDidDisconnect:)]) {
            [_delegate gamepadDidDisconnect:self];
		}
    }
}

- (NSString *) vendorProductString
{
    return [NSString stringWithFormat:@"%04lx:%04lx",
            (long)[self vendorId], (long)[self productId]];
}

- (NSInteger) vendorProductId
{
    return (_vendorId << 16) | _productId;
}

- (NSString *) description
{
    return [NSString stringWithFormat:@"%@ (0x%04lx:0x%04lx)",
            [self name], (long)[self vendorId], (long)[self productId]];
}

- (void) didReceiveInputValue:(IOHIDValueRef) valueRef
{
    IOHIDElementRef element = IOHIDValueGetElement(valueRef);
    
    NSInteger usagePage = IOHIDElementGetUsagePage(element);
    NSInteger usage = IOHIDElementGetUsage(element);
    NSInteger value = IOHIDValueGetIntegerValue(valueRef);
    
    if (usagePage == kHIDPage_GenericDesktop) {
        if (usage == kHIDUsage_GD_X) {
            NSInteger min = IOHIDElementGetLogicalMin(element);
            NSInteger max = IOHIDElementGetLogicalMax(element);
            NSInteger center = (max + min) / 2;
            NSInteger range = max - min;
            
            if (labs(value) < range * .3) {
                value = 0;
            }
            
            if (_axes.x != value) {
                _axes.x = value;
                if ([_delegate respondsToSelector:@selector(gamepad:xChanged:center:eventData:)]) {
                    AKGamepadEventData *eventData = [[AKGamepadEventData alloc] init];
                    [eventData setSourceId:IOHIDElementGetCookie(element)];
                    
                    [_delegate gamepad:self
                              xChanged:value
                                center:center
                             eventData:eventData];
                }
            }
        } else if (usage == kHIDUsage_GD_Y) {
            NSInteger min = IOHIDElementGetLogicalMin(element);
            NSInteger max = IOHIDElementGetLogicalMax(element);
            NSInteger center = (max + min) / 2;
            NSInteger range = max - min;
            
            if (labs(value) < range * .3) {
                value = 0;
            }
            
            if (_axes.y != value) {
                _axes.y = value;
                if ([_delegate respondsToSelector:@selector(gamepad:yChanged:center:eventData:)]) {
                    AKGamepadEventData *eventData = [[AKGamepadEventData alloc] init];
                    [eventData setSourceId:IOHIDElementGetCookie(element)];
                    
                    [_delegate gamepad:self
                              yChanged:value
                                center:center
                             eventData:eventData];
                }
            }
        }
    } else if (usagePage == kHIDPage_Button) {
		if ([_delegate respondsToSelector:@selector(gamepad:button:isDown:eventData:)]) {
			AKGamepadEventData *eventData = [[AKGamepadEventData alloc] init];
			[eventData setSourceId:IOHIDElementGetCookie(element)];
			
			[_delegate gamepad:self
						button:usage
						isDown:(value & 1)
					 eventData:eventData];
		}
    }
}

- (NSMutableDictionary *) currentAxisValues
{
    CFArrayRef elements = IOHIDDeviceCopyMatchingElements(hidDevice, NULL, kIOHIDOptionsTypeNone);
    NSArray *elementArray = (__bridge NSArray *) elements;
    
    NSMutableDictionary *axesAndValues = [[NSMutableDictionary alloc] init];
    
    [elementArray enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
         IOHIDElementRef element = (__bridge IOHIDElementRef) obj;
         
         NSInteger usagePage = IOHIDElementGetUsagePage(element);
         if (usagePage == kHIDPage_GenericDesktop) {
             IOHIDElementType type = IOHIDElementGetType(element);
             if (type == kIOHIDElementTypeInput_Misc || type == kIOHIDElementTypeInput_Axis) {
                 NSInteger usage = IOHIDElementGetUsage(element);
                 if (usage == kHIDUsage_GD_X || usage == kHIDUsage_GD_Y) {
                     IOHIDValueRef tIOHIDValueRef;
                     if (IOHIDDeviceGetValue(hidDevice, element, &tIOHIDValueRef) == kIOReturnSuccess) {
                         IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
                         NSInteger integerValue = IOHIDValueGetIntegerValue(tIOHIDValueRef);
                         
                         [axesAndValues setObject:@(integerValue)
                                           forKey:@((NSInteger)cookie)];
                     }
                 }
             }
         }
     }];
    
    return axesAndValues;
}

@end

#pragma mark - IOHID C Callbacks

static void gamepadInputValueCallback(void *context, IOReturn result, void *sender, IOHIDValueRef value)
{
    @autoreleasepool {
        [(__bridge AKGamepad *) context didReceiveInputValue:value];
    }
}
