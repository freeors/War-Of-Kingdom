#include "../../SDL_internal.h"
#include "SDL_video.h"
#include "SDL_timer.h"

#import "SDL_uikitble.h"


#define MAX_BLE_PERIPHERALS			48




static NSString *const kCharacteristicUUID = @"00002A2B-0000-1000-8000-00805F9B34FB";
// static NSString *const kServiceUUID = @"00001805-0000-1000-8000-00805F9B34FB";
static NSString *const kServiceUUID = @"5A5B1805-0000-1000-8000-00805F9B34FB";

// static NSString *const kCharacteristicUUID = @"2A2B";
// static NSString *const kServiceUUID = @"1805";


static SDL_uikitble* ble = NULL;
static SDL_BlePeripheral ble_peripherals[MAX_BLE_PERIPHERALS];
static int valid_ble_peripherals = 0;
static SDL_BleCallbacks* current_callbacks = NULL;

static SDL_BlePeripheral* find_peripheral_from_cookie(const void* cookie)
{
	SDL_BlePeripheral* peripheral = NULL;
	for (int at = 0; at < valid_ble_peripherals; at ++) {
        peripheral = ble_peripherals + at;
        if (peripheral->cookie == cookie) {
            return peripheral;
        }
	}
	return NULL;
}

static SDL_BleService* find_service(SDL_BlePeripheral* device, const char* uuid)
{
	SDL_BleService* tmp;
	for (int at = 0; at < device->valid_services; at ++) {
		tmp = device->services + at;
		if (!SDL_memcmp(tmp->uuid, uuid, strlen(uuid))) {
			return tmp;
		}
	}
	return NULL;
}

static SDL_BleCharacteristic* find_characteristic(SDL_BlePeripheral* device, const char* service_uuid, const char* uuid)
{
	SDL_BleService* service = find_service(device, service_uuid);

	SDL_BleCharacteristic* tmp;
	for (int at = 0; at < service->valid_characteristics; at ++) {
        tmp = service->characteristics + at;
        if (!SDL_memcmp(tmp->uuid, uuid, strlen(uuid))) {
            return tmp;
        }
	}
	return NULL;
}

static SDL_BleCharacteristic* find_characteristic2(const void* cookie, const char* uuid)
{
    SDL_BlePeripheral* device = find_peripheral_from_cookie(cookie);
    
	SDL_BleService* service;
	SDL_BleCharacteristic* tmp;
	for (int at = 0; at < device->valid_services; at ++) {
		service = device->services + at;
		for (int at2 = 0; at2 < service->valid_characteristics; at2 ++) {
			tmp = service->characteristics + at2;
			if (!SDL_memcmp(tmp->uuid, uuid, strlen(uuid))) {
				return tmp;
			}
		}	
	}
	return NULL;
}

static SDL_BlePeripheral* discoverPeripheral(void* cookie, const char* name, int rssi)
{
	// NSLog(@"discover peripheral: %s", name? name: "<nil>");

    SDL_BlePeripheral* peripheral = NULL;
	for (int at = 0; at < valid_ble_peripherals; at ++) {
        peripheral = ble_peripherals + at;
        if (peripheral->cookie == cookie) {
            return peripheral;
        }
	}

	peripheral = ble_peripherals + valid_ble_peripherals;
    memset(peripheral, 0, sizeof(SDL_BlePeripheral));
    
	// field: name
	size_t s;
	if (!name || name[0] == '\0') {
		s = 0;
	} else {
		s = SDL_strlen(name);
	}
	peripheral->name = (char*)SDL_malloc(s + 1);
	if (s) {
		memcpy(peripheral->name, name, s);
	}
	peripheral->name[s] = '\0';

	valid_ble_peripherals ++;

	return peripheral;
}

static void discoverServices(SDL_BlePeripheral* device)
{
	CBPeripheral* peripheral = (__bridge CBPeripheral *)(device->cookie);
	if (device->services) {
		free(device->services);
		device->services = NULL;
	}

	device->valid_services = 0;
	for (CBService *service in peripheral.services) {
		device->valid_services ++;
	}

	if (device->valid_services) {
		device->services = (SDL_BleService*)SDL_malloc(device->valid_services * sizeof(SDL_BleService));
		memset(device->services, 0, device->valid_services * sizeof(SDL_BleService));

		size_t s;
		int at = 0;
		for (CBService *service in peripheral.services) {
			SDL_BleService* tmp = device->services + at;
            const char* cstr = [service.UUID.UUIDString UTF8String];
            s = SDL_strlen(cstr);
			tmp->uuid = (char*)malloc(s + 1);
			memcpy(tmp->uuid, cstr, s);
			tmp->uuid[s] = '\0';

			at ++;
		}
	}
}

static void free_characteristics(SDL_BleService* service)
{
    if (service->characteristics) {
        for (int at = 0; at < service->valid_characteristics; at ++) {
            SDL_BleCharacteristic* characteristic = service->characteristics + at;
            free(characteristic->uuid);
            characteristic->characteristic = NULL;
        }
        free(service->characteristics);
        service->characteristics = NULL;
        service->valid_characteristics = 0;
    }
}

static void free_service(SDL_BleService* service)
{
    free_characteristics(service);
    if (service->uuid) {
        free(service->uuid);
    }
}

static void discoverCharacteristics(SDL_BlePeripheral* device, CBService* service)
{
	const char* service_uuid = [service.UUID.UUIDString UTF8String];
	SDL_BleService* service2 = find_service(device, service_uuid);
	if (!service2) {
		return;
	}

    free_characteristics(service2);
	for (CBCharacteristic* characteristic in service.characteristics) {
		service2->valid_characteristics ++;
	}
	service2->characteristics = (SDL_BleCharacteristic*)SDL_malloc(service2->valid_characteristics * sizeof(SDL_BleCharacteristic));
	memset(service2->characteristics, 0, service2->valid_characteristics * sizeof(SDL_BleCharacteristic));

    int at = 0;
    size_t s;
	for (CBCharacteristic* characteristic in service.characteristics) {
		SDL_BleCharacteristic* tmp = service2->characteristics + at;
        const char* uuid = [characteristic.UUID.UUIDString UTF8String];
        s = strlen(uuid);
        tmp->uuid = (char*)malloc(s + 1);
        memcpy(tmp->uuid, uuid, s);
        tmp->uuid[s] = '\0';
		// tmp->characteristic = (void *)CFBridgingRetain(characteristic);
		tmp->characteristic = (__bridge void *)(characteristic);
        at ++;
	}
}

static void release_peripheral(SDL_BlePeripheral* device)
{
    NSLog(@"release peripheral, name: %s, cookie: %p", device->name, device->cookie);
    
    int at = 0;
    for (at = 0; at < valid_ble_peripherals; at ++) {
        SDL_BlePeripheral* tmp = ble_peripherals + at;
        if (tmp == device) {
            break;
        }
    }
    if (at == valid_ble_peripherals) {
        return;
    }

    // if this peripheral is connected, disconnect first!
    CBPeripheral* peripheral = (__bridge CBPeripheral*)(device->cookie);
    int state = [peripheral state];
    if (state == CBPeripheralStateConnected || device->connected) {
        // user may poweroff ble directly. in this case, state return CBPeripheralStateDisconnected as if it is connecting.
        device->connected = false;
        if (current_callbacks && current_callbacks->disconnect_peripheral) {
            current_callbacks->disconnect_peripheral(device, 0);
        }
    }

    const void* cookie = device->cookie;
    
    if (current_callbacks && current_callbacks->release_peripheral) {
        current_callbacks->release_peripheral(cookie);
    }
    
    if (device->name) {
        SDL_free(device->name);
        device->name = NULL;
    }
    if (device->services) {
        for (int at2 = 0; at2 < device->valid_services; at2 ++) {
            free_service(device->services + at2);
        }
        SDL_free(device->services);
    }
    
    
    
    CFRelease(device->cookie);
    device->cookie = NULL;
    
    if (at < valid_ble_peripherals - 1) {
        memcpy(&(ble_peripherals[at]), &(ble_peripherals[at + 1]), (valid_ble_peripherals - at - 1) * sizeof(SDL_BlePeripheral));
    }
    
    valid_ble_peripherals --;
}

static void release_peripherals()
{
	while (valid_ble_peripherals) {
		SDL_BlePeripheral* device = ble_peripherals;
        release_peripheral(device);
	}
    
    int ii = 0;
	// valid_ble_peripherals = NULL;
	// ble = NULL;
}

@implementation SDL_uikitble
/*
@synthesize bleManager;

@synthesize peripheraManager;
@synthesize customerCharacteristic;
@synthesize customerService;
*/
// #define currChannel [babySpeaker callbackOnCurrChannel]

+(instancetype)shareSDL_uikitble{
    static SDL_uikitble *share = nil;
    static dispatch_once_t oneToken;
    dispatch_once(&oneToken, ^{
        share = [[SDL_uikitble alloc]init];
    });
    
    return share;
}

-(instancetype)init{
    self = [super init];
    if (self){

        // bleManager = [[CBCentralManager alloc]initWithDelegate:self queue:dispatch_get_main_queue()];
        // peripheraManager = [[CBPeripheralManager alloc]initWithDelegate:self queue:dispatch_get_main_queue()];
    }
    return  self;
}

-(void)initCentralManager
{
    if (!bleManager) {
        bleManager = [[CBCentralManager alloc]initWithDelegate:self queue:dispatch_get_main_queue()];
    }
}

-(void)initPeripheralManager
{
    if (!peripheralManager) {
        peripheralManager = [[CBPeripheralManager alloc]initWithDelegate:self queue:dispatch_get_main_queue()];
    }
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral{
    switch (peripheral.state) {
            //在这里判断蓝牙设别的状态  当开启了则可调用  setUp方法(自定义)
        case CBPeripheralManagerStatePoweredOn:
            NSLog(@"powered on");
            break;
        case CBPeripheralManagerStatePoweredOff:
            NSLog(@"powered off");
            break;
            
        default:
            break;
    }
    
    if (peripheral.state == CBCentralManagerStatePoweredOn) {
        [self setUp];
    }
}

-(void)setUp
{
/*
    [peripheralManager startAdvertising:@{CBAdvertisementDataLocalNameKey:@"libSDL",
                                          CBAdvertisementDataServiceUUIDsKey:@[[CBUUID UUIDWithString:kServiceUUID]]}];
    return;
*/
    CBUUID *characteristicUUID = [CBUUID UUIDWithString:kCharacteristicUUID];
    customerCharacteristic = [[CBMutableCharacteristic alloc]initWithType:characteristicUUID properties:(CBCharacteristicPropertyRead | CBCharacteristicPropertyNotify) value:nil permissions:CBAttributePermissionsReadable];
    
    CBUUID *serviceUUID = [CBUUID UUIDWithString:kServiceUUID];
    customerService = [[CBMutableService alloc]initWithType:serviceUUID primary:YES];
    
    // [customerService setCharacteristics:@[characteristicUUID]];
    [customerService setCharacteristics:@[customerCharacteristic]];
    
    //添加后就会调用代理的- (void)peripheralManager:(CBPeripheralManager *)peripheral didAddService:(CBService *)service error:(NSError *)error
    [peripheralManager addService:customerService];
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral error:(NSError *)error{
    NSLog(@"in peripheralManagerDidStartAdvertisiong:error");
}

-(void)updateCharacteristicValue
{
    NSLog(@"in peripheralManagerDidStartAdvertisiong:error");
    
    // 特征值
    // NSString *valueStr=[NSString stringWithFormat:@"%@ --%@",kPeripheralName,[NSDate   date]];
    // NSData *value=[valueStr dataUsingEncoding:NSUTF8StringEncoding];
    //更新特征值
    // [self.peripheraManager updateValue:value forCharacteristic:self.characteristicM onSubscribedCentrals:nil];
    // [self writeToLog:[NSString stringWithFormat:@"更新特征值：%@",valueStr]];
}

- (BOOL)updateValue:(NSData *)value forCharacteristic:(CBMutableCharacteristic *)characteristic onSubscribedCentrals:(NSArray *)centrals
{
    NSLog(@"in onSubscribedCentrals:centrals");
    return true;
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral didAddService:(CBService *)service error:(NSError *)error
{
    if (!error) {
        //starts advertising the service
        [peripheralManager startAdvertising:@{CBAdvertisementDataLocalNameKey:@"libSDL",
                                                  CBAdvertisementDataServiceUUIDsKey:@[[CBUUID UUIDWithString:kServiceUUID]]}];
    } else {
        NSLog(@"addService with error: %@", [error localizedDescription]);
    }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral didReceiveReadRequest:(CBATTRequest *)request
{
    NSLog(@"in peripheralManagerDidStartAdvertisiong:error");
    if ([request.characteristic.UUID isEqual:customerCharacteristic.UUID]) {
        // request.offset == 0
        if (request.offset != 0) {
            [peripheralManager respondToRequest:request
                                       withResult:CBATTErrorInvalidOffset];
            return;
        }
/*
        request.value = [customerCharacteristic.value
                         subdataWithRange:NSMakeRange(request.offset,
                                                      customerCharacteristic.value.length - request.offset)];
*/
        NSDate* now = [NSDate date];
        
        [peripheralManager respondToRequest:request withResult:CBATTErrorSuccess];
        
    } else {
        [peripheralManager respondToRequest:request withResult:CBATTErrorRequestNotSupported];
    }
}

#pragma mark -接收到通知

//开始扫描
-(void)scanForPeripheralNotifyReceived:(NSNotification *)notify{
//    NSLog(@">>>scanForPeripheralsNotifyReceived");
}

//扫描到设备
-(void)didDiscoverPeripheralNotifyReceived:(NSNotification *)notify{
//    CBPeripheral *peripheral =[notify.userInfo objectForKey:@"peripheral"];
//    NSLog(@">>>didDiscoverPeripheralNotifyReceived:%@",peripheral.name);
}

//开始连接设备
-(void)connectToPeripheralNotifyReceived:(NSNotification *)notify{
//    NSLog(@">>>connectToPeripheralNotifyReceived");
}

//扫描Peripherals
-(void)scanPeripherals{
    [bleManager scanForPeripheralsWithServices:nil options:@{CBCentralManagerScanOptionAllowDuplicatesKey:@YES}];
}
//连接Peripherals
-(void)connectToPeripheral:(CBPeripheral *)peripheral{
    [bleManager connectPeripheral:peripheral options:nil];
    //

}

//断开所以已连接的设备
-(void)stopConnectAllPerihperals{
/*
    for (int i=0;i<connectedPeripherals.count;i++) {
        [bleManager cancelPeripheralConnection:connectedPeripherals[i]];
    }
    connectedPeripherals = [[NSMutableArray alloc]init];
*/
    NSLog(@">>>babyBluetooth stop");
}
//停止扫描
-(void)stopScan{
    [bleManager stopScan];
}

#pragma mark -CBCentralManagerDelegate委托方法

- (void)centralManagerDidUpdateState:(CBCentralManager *)central{
 
    switch (central.state) {
        case CBCentralManagerStateUnknown:
            NSLog(@">>>CBCentralManagerStateUnknown");
            break;
        case CBCentralManagerStateResetting:
            NSLog(@">>>CBCentralManagerStateResetting");
            break;
        case CBCentralManagerStateUnsupported:
            NSLog(@">>>CBCentralManagerStateUnsupported");
            break;
        case CBCentralManagerStateUnauthorized:
            NSLog(@">>>CBCentralManagerStateUnauthorized");
            break;
        case CBCentralManagerStatePoweredOff:
            NSLog(@">>>CBCentralManagerStatePoweredOff");
            break;
        case CBCentralManagerStatePoweredOn:
            NSLog(@">>>CBCentralManagerStatePoweredOn");
            break;
        default:
            break;
    }
	if (central.state == CBCentralManagerStatePoweredOn) {
		[self scanPeripherals];

	} else if (central.state == CBCentralManagerStatePoweredOff) {
		release_peripherals();
	}
}

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary *)advertisementData RSSI:(NSNumber *)RSSI
{
	SDL_BlePeripheral* device = discoverPeripheral((__bridge void *)(peripheral), [peripheral.name UTF8String], [RSSI intValue]);
	if (!device->cookie) {
		device->cookie = (void *)CFBridgingRetain(peripheral);
		device->last_active = SDL_GetTicks();

		NSLog(@"peripheral: %@ ------", peripheral.name);
		for (id key in advertisementData) {
			NSLog(@"key: %@ ,value: %@", key, [advertisementData objectForKey:key]);
		}
    
		NSData* mac_addr = [advertisementData objectForKey:@"kCBAdvDataManufacturerData"];
		const unsigned char* mac_addr_str = NULL;
		if (mac_addr && [mac_addr length] == 8) {
			mac_addr_str = [mac_addr bytes];
			device->mac_addr[0] = mac_addr_str[2];
			device->mac_addr[1] = mac_addr_str[3];
			device->mac_addr[2] = mac_addr_str[4];
			device->mac_addr[3] = mac_addr_str[5];
			device->mac_addr[4] = mac_addr_str[6];
			device->mac_addr[5] = mac_addr_str[7];
		} else {
			device->mac_addr[0] = '\0';
		}
	}

    if (current_callbacks && current_callbacks->discover_peripheral) {
        current_callbacks->discover_peripheral(device);
    }
    
/*
    //发出通知
    [[NSNotificationCenter defaultCenter]postNotificationName:@"didDiscoverPeripheral"
                                                       object:nil
                                                     userInfo:@{@"central":central,@"peripheral":peripheral,@"advertisementData":advertisementData,@"RSSI":RSSI}];
*/
}

//停止扫描
-(void)disconnect:(id)sender
{
    [bleManager stopScan];
}

//连接到Peripherals-成功
- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral
{
    SDL_BlePeripheral* device = find_peripheral_from_cookie((__bridge void *)(peripheral));
    device->connected = true;
    if (current_callbacks && current_callbacks->connect_peripheral) {
		current_callbacks->connect_peripheral(device, 0);
	}

    //扫描外设的服务
    [peripheral setDelegate:self];
}

//连接到Peripherals-失败
-(void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
    SDL_BlePeripheral* device = find_peripheral_from_cookie((__bridge void *)(peripheral));
    NSLog(@">>>connect to（%@）fail, readon:%@", [peripheral name], [error localizedDescription]);
	if (current_callbacks && current_callbacks->connect_peripheral) {
		current_callbacks->connect_peripheral(device, -1 * EFAULT);
	}
}

// Peripherals断开连接
- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
    // both normal and except disconnect will enter it.
    SDL_BlePeripheral* device = find_peripheral_from_cookie((__bridge void *)(peripheral));
    device->connected = false;
    int error2 = 0;
    if (error) {
        NSLog(@">>>disconnect peripheral（%@）, reason:%@",[peripheral name], [error localizedDescription]);
        error2 = -1 * EFAULT;
    }
    if (current_callbacks && current_callbacks->disconnect_peripheral) {
        current_callbacks->disconnect_peripheral(device, error2);
    }
    if (error) {
        // except disconnect, thank lost.
        release_peripheral(device);
    }
}

// 扫描到服务
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error
{
    NSLog(@">>>扫描到服务：%@", peripheral.services);
	SDL_BlePeripheral* device = find_peripheral_from_cookie((__bridge void *)(peripheral));
	int error2 = 0;
    if (!error) {
		discoverServices(device);

    } else {
		NSLog(@">>>Discovered services for %@ with error: %@", peripheral.name, [error localizedDescription]);
        error2 = -1 * EFAULT;
	}

    if (current_callbacks && current_callbacks->discover_services) {
        current_callbacks->discover_services(device, error2);
    }

    //discover characteristics
    // if (needDiscoverCharacteristics) {
    //    for (CBService *service in peripheral.services) {
    //        [peripheral discoverCharacteristics:nil forService:service];
    //    }
    // }
}


//发现服务的Characteristics
-(void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error
{
    SDL_BlePeripheral* device = find_peripheral_from_cookie((__bridge void *)(peripheral));
	int error2 = 0;
    if (!error) {
		discoverCharacteristics(device, service);

	} else {
        // NSLog(@"error Discovered characteristics for %@ with error: %@", service.UUID, [error localizedDescription]);
        error2 = -1 * EFAULT;
    }

	if (current_callbacks && current_callbacks->discover_characteristics) {
        current_callbacks->discover_characteristics(device, find_service(device, [service.UUID.UUIDString UTF8String]), error2);
    }
}

//读取Characteristics的值
-(void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    SDL_BlePeripheral* device = find_peripheral_from_cookie((__bridge void *)(peripheral));
    NSData* data;
    if (error) {
        // NSLog(@"error didUpdateValueForCharacteristic %@ with error: %@", characteristic.UUID, [error localizedDescription]);
        
    } else {
		// NSLog(@"didUpdateValueForCharacteristic %@", characteristic.UUID);
        data = characteristic.value;
/*
		// Byte
		unsigned char *testByte = (unsigned char*)[data bytes];

        char exp = testByte[4];
        double temp = (testByte[1] + (testByte[2] << 8) + (testByte[3] << 16)) * pow(10, exp);
        printf("len: %d, flags: %d, temp: %7.2f, (%d %d %d %d)\n", [data length], testByte[0], temp, testByte[1], testByte[2], testByte[3], testByte[4]);
 */
	}

	if (current_callbacks && current_callbacks->read_characteristic) {
        current_callbacks->read_characteristic(device, find_characteristic2((__bridge void *)(peripheral), [characteristic.UUID.UUIDString UTF8String]), (unsigned char*)[data bytes], (int)[data length]);
    }
}
//发现Characteristics的Descriptors
-(void)peripheral:(CBPeripheral *)peripheral didDiscoverDescriptorsForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
	SDL_BlePeripheral* device = find_peripheral_from_cookie((__bridge void *)(peripheral));
    NSData* data;
	int error2 = 0;
    if (error) {
        NSLog(@"error Discovered DescriptorsForCharacteristic for %@ with error: %@", characteristic.UUID, [error localizedDescription]);
        error2 = -1 * EFAULT;
        
    } else {
        data = characteristic.value;
	}

	if (current_callbacks && current_callbacks->discover_descriptors) {
        current_callbacks->discover_descriptors(device, find_characteristic2((__bridge void *)(peripheral), [characteristic.UUID.UUIDString UTF8String]), error2);
    }

    for (CBDescriptor *d in characteristic.descriptors) {
        error2 = 0;
        // [peripheral readValueForDescriptor:d];
    }
    
/*
    //执行一次的方法
    if (oneReadValueForDescriptors) {
        for (CBDescriptor *d in characteristic.descriptors)
        {
            [peripheral readValueForDescriptor:d];
        }
        oneReadValueForDescriptors = NO;
    }
*/
}

//读取Characteristics的Descriptors的值
-(void)peripheral:(CBPeripheral *)peripheral didUpdateValueForDescriptor:(CBDescriptor *)descriptor error:(NSError *)error{

    if (error)
    {
        NSLog(@"error didUpdateValueForDescriptor for %@ with error: %@", descriptor.UUID, [error localizedDescription]);
        return;
    }
/*    //回叫block
    if ([currChannel blockOnReadValueForDescriptors]) {
        [currChannel blockOnReadValueForDescriptors](peripheral,descriptor,error);
    }
*/
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    SDL_BlePeripheral* device = find_peripheral_from_cookie((__bridge void *)(peripheral));
    int error2 = 0;
    if (error) {
        NSLog(@"Changing notification state for %@ with error: %@", characteristic.UUID, [error localizedDescription]);
        error2 = -1 * EFAULT;
    }
    if (current_callbacks && current_callbacks->notify_characteristic) {
        current_callbacks->notify_characteristic(device, find_characteristic2((__bridge void *)(peripheral), [characteristic.UUID.UUIDString UTF8String]), error2);
    }
}


-(void)peripheral:(CBPeripheral *)peripheral didWriteValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    SDL_BlePeripheral* device = find_peripheral_from_cookie((__bridge void *)(peripheral));
    int error2 = 0;
    if (error) {
        NSLog(@"Write characterstic for %@ with error: %@", characteristic.UUID, [error localizedDescription]);
        error2 = -1 * EFAULT;
    }

    if (current_callbacks && current_callbacks->write_characteristic) {
        current_callbacks->write_characteristic(device, find_characteristic2((__bridge void *)(peripheral), [characteristic.UUID.UUIDString UTF8String]), error2);
    }

}

-(void)peripheral:(CBPeripheral *)peripheral didWriteValueForDescriptor:(CBDescriptor *)descriptor error:(NSError *)error{
//    NSLog(@">>>didWriteValueForCharacteristic");
//    NSLog(@">>>uuid:%@,new value:%@",descriptor.UUID,descriptor.value);
}

@end

void UIKit_ScanPeripherals(SDL_BleCallbacks* callbacks)
{
    if (!ble) {
        ble = [SDL_uikitble shareSDL_uikitble];
    }
        
	current_callbacks = callbacks;
	if (!ble->bleManager) {
		memset(&ble_peripherals, 0, sizeof(ble_peripherals));
		[ble initCentralManager];
    } else {
        [ble scanPeripherals];
    }
/*
    for (int at = 0; at < valid_ble_peripherals; at ++) {
        SDL_BlePeripheral* peripheral = ble_peripherals + at;
        if (current_callbacks && current_callbacks->discover_peripheral) {
            current_callbacks->discover_peripheral(peripheral);
        }
    }
*/
}

void UIKit_StopScanPeripherals()
{
    if (!ble) {
        return;
    }
    [ble->bleManager stopScan];
}

SDL_BlePeripheral* UIKit_FindBlePeripheral(const void* cookie)
{
    return find_peripheral_from_cookie(cookie);
}

void UIKit_StartAdvertise()
{
    if (!ble) {
        ble = [SDL_uikitble shareSDL_uikitble];
    }
    
    if (!ble->peripheralManager) {
        [ble initPeripheralManager];
    }
}

void UIKit_ConnectPeripheral(void* cookie)
{
	SDL_BlePeripheral* peripheral = find_peripheral_from_cookie(cookie);
	if (peripheral) {
		CBPeripheral *data = (__bridge CBPeripheral *)peripheral->cookie;
		[ble connectToPeripheral:(CBPeripheral*)data];
        return;
	}

	if (current_callbacks && current_callbacks->connect_peripheral) {
		current_callbacks->connect_peripheral(peripheral, -1 * EEXIST);
	}
}

void UIKit_DisconnectPeripheral(void* cookie)
{
    SDL_BlePeripheral* peripheral = find_peripheral_from_cookie(cookie);
    if (peripheral) {
        CBPeripheral *data = (__bridge CBPeripheral *)peripheral->cookie;
        [ble->bleManager cancelPeripheralConnection:(CBPeripheral*)data];
        return;
    }
    
    if (current_callbacks && current_callbacks->disconnect_peripheral) {
        current_callbacks->disconnect_peripheral(peripheral, -1 * EEXIST);
    }
}

void UIKit_GetServices(void* cookie)
{
	SDL_BlePeripheral* peripheral = find_peripheral_from_cookie(cookie);
	if (peripheral) {
		CBPeripheral *data = (__bridge CBPeripheral *)peripheral->cookie;
		[data discoverServices:nil];
        return;
	}

	if (current_callbacks && current_callbacks->discover_services) {
		current_callbacks->discover_services(peripheral, -1 * EEXIST);
	}
}

void UIKit_GetCharacteristics(const void* cookie, const char* service_uuid)
{
	SDL_BlePeripheral* device = find_peripheral_from_cookie(cookie);
	if (device) {
		CBPeripheral *peripheral = (__bridge CBPeripheral *)device->cookie;
		for (CBService *service in peripheral.services) {
            if (!SDL_memcmp([service.UUID.UUIDString UTF8String], service_uuid, strlen(service_uuid))) {
				[peripheral discoverCharacteristics:nil forService:service];
				return;
			}
		}
	}

	if (current_callbacks && current_callbacks->discover_characteristics) {
		current_callbacks->discover_characteristics(device, find_service(device, service_uuid), -1 * EEXIST);
	}
}

void UIKit_ReadCharacteristic(const void* cookie, const char* characteristic_uuid)
{
    SDL_BlePeripheral* device = find_peripheral_from_cookie(cookie);
    SDL_BleCharacteristic* characteristic = find_characteristic2(cookie, characteristic_uuid);
	if (characteristic) {
		CBPeripheral* peripheral = (__bridge CBPeripheral *)cookie;
		CBCharacteristic* characteristic2 = (__bridge CBCharacteristic *)characteristic->characteristic;
		[peripheral readValueForCharacteristic:characteristic2];
		return;		
	}
	if (current_callbacks && current_callbacks->read_characteristic) {
		current_callbacks->read_characteristic(device, characteristic, NULL, 0);
	}
}

void UIKit_SetNotify(const void* cookie, const char* characteristic_uuid)
{
    SDL_BlePeripheral* device = find_peripheral_from_cookie(cookie);
	SDL_BleCharacteristic* characteristic = find_characteristic2(cookie, characteristic_uuid);
	if (characteristic) {
		CBPeripheral* peripheral = (__bridge CBPeripheral *)cookie;
		CBCharacteristic* characteristic2 = (__bridge CBCharacteristic *)characteristic->characteristic;
		[peripheral setNotifyValue:YES forCharacteristic:characteristic2];
		return;
	}
    if (current_callbacks && current_callbacks->notify_characteristic) {
        current_callbacks->notify_characteristic(device, characteristic, -1 * EEXIST);
    }
}

void UIKit_WriteCharacteristic(const void* cookie, const char* characteristic_uuid, const char* data, int size)
{
    SDL_BlePeripheral* device = find_peripheral_from_cookie(cookie);
    SDL_BleCharacteristic* characteristic = find_characteristic2(cookie, characteristic_uuid);
    
    if (characteristic) {
        CBPeripheral* peripheral = (__bridge CBPeripheral *)cookie;
        CBCharacteristic* characteristic2 = (__bridge CBCharacteristic *)characteristic->characteristic;
        NSData* data2 = [NSData dataWithBytes:data length:size];
        [peripheral writeValue:data2 forCharacteristic:characteristic2 type:CBCharacteristicWriteWithResponse];
        return;
    }
    if (current_callbacks && current_callbacks->write_characteristic) {
        current_callbacks->write_characteristic(device, characteristic, -1 * EEXIST);
    }
}

void UIKit_DiscoverDescriptors(const void* cookie, const char* characteristic_uuid)
{
    {
        int ii = 0;
        SDL_BlePeripheral* device = find_peripheral_from_cookie(cookie);
        release_peripheral(device);
        return;
    }
    
	SDL_BleCharacteristic* characteristic = find_characteristic2(cookie, characteristic_uuid);
	if (characteristic) {
		CBPeripheral* peripheral = (__bridge CBPeripheral *)cookie;
		CBCharacteristic* characteristic2 = (__bridge CBCharacteristic *)characteristic->characteristic;
		[peripheral discoverDescriptorsForCharacteristic:characteristic2];
		return;
	}
}

int UIKit_AuthorizationStatus()
{
    CBPeripheralManagerAuthorizationStatus status = [CBPeripheralManager authorizationStatus];
    int retval = status;
    return retval;
}