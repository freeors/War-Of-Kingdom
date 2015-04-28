#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

typedef struct {
	char* uuid;
	void* characteristic;
} SDL_BleCharacteristic;

typedef struct {
	char* uuid;
	SDL_BleCharacteristic* characteristics;
	int valid_characteristics;
} SDL_BleService;

typedef struct {
	char* name;
    unsigned char mac_addr[6];
	SDL_BleService* services;
	int valid_services;
    
    int connected;
    unsigned int last_active;

	void* cookie;
} SDL_BlePeripheral;

typedef struct {
    void (*discover_peripheral)(SDL_BlePeripheral* peripheral);
    void (*release_peripheral)(const void* cookie);
    void (*connect_peripheral)(SDL_BlePeripheral* peripheral, const int error);
    void (*disconnect_peripheral)(SDL_BlePeripheral* peripheral, const int error);
	void (*discover_services)(SDL_BlePeripheral* peripheral, const int error);
	void (*discover_characteristics)(SDL_BlePeripheral* peripheral, SDL_BleService* service, const int error);
    void (*read_characteristic)(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, unsigned char* data, int len);
    void (*write_characteristic)(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const int error);
    void (*notify_characteristic)(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const int error);
	void (*discover_descriptors)(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const int error);
} SDL_BleCallbacks;

@interface SDL_uikitble : NSObject<CBCentralManagerDelegate, CBPeripheralDelegate, CBPeripheralManagerDelegate> {
    
@public
    // 主设备
    CBCentralManager* bleManager;
    
    // @property(strong,nonatomic) CBCentralManager* bleManager;
    
    // @property(strong,nonatomic) CBPeripheralManager *peripheraManager;
    // @property(strong,nonatomic) CBMutableCharacteristic *customerCharacteristic;
    // @property (strong,nonatomic) CBMutableService *customerService;

    CBPeripheralManager* peripheralManager;
    CBMutableCharacteristic* customerCharacteristic;
    CBMutableService* customerService;
}



//扫描Peripherals
-(void)scanPeripherals;
//连接Peripherals
-(void)connectToPeripheral:(CBPeripheral *)peripheral;
//断开所以已连接的设备
-(void)stopConnectAllPerihperals;
//停止扫描
-(void)stopScan;

/**
 * 单例构造方法
 * @return BabyBluetooth共享实例
 */
+(instancetype)shareSDL_uikitble;

@end



