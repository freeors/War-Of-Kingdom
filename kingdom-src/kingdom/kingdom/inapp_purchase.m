//
//  inapp_purchase.m
//

#import <Foundation/Foundation.h>
#import <StoreKit/StoreKit.h>
#import "UIKit/UIKit.h"

extern void inapp_purchase_cb(const char* identifier, int complete);

SKProduct * proUpgradeProduct;
void startAppPayment();
// #define kInAppPurchaseProUpgradeProductId @"com.ancientcc.testwin.getcard"
#define kInAppPurchaseProUpgradeProductId @"com.ancientcc.testwin.tacticslot"

@interface InAppPurchaseManager : NSObject <SKPaymentTransactionObserver,SKProductsRequestDelegate> {
    
}
//// SKPaymentTransactionObserver
- (void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray *)transactions;

//// SKProductsRequestDelegate
- (void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response;
@end

@implementation InAppPurchaseManager
////SKPaymentTransactionObserver
- (void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray *)transactions
{
    for (SKPaymentTransaction *transaction in transactions)
    {
        switch (transaction.transactionState)
        {
            case SKPaymentTransactionStatePurchased:
                [self completeTransaction:transaction];
                break;
            case SKPaymentTransactionStateFailed:
                [self failedTransaction:transaction];
                break;
            case SKPaymentTransactionStateRestored:
                [self restoreTransaction:transaction];
                break;
            default:
                break;
        }
    }
}

//
// removes the transaction from the queue and posts a notification with the transaction result
//
- (void)finishTransaction:(SKPaymentTransaction *)transaction wasSuccessful:(BOOL)wasSuccessful
{
    // remove the transaction from the payment queue.
    [[SKPaymentQueue defaultQueue] finishTransaction:transaction];
    
    inapp_purchase_cb([transaction.payment.productIdentifier UTF8String], wasSuccessful? 1: 0);
}

//
// called when the transaction was successful
//
- (void)completeTransaction:(SKPaymentTransaction *)transaction
{
	NSLog(@"completeTransaction...%@", transaction.payment.productIdentifier);

    [self finishTransaction:transaction wasSuccessful:YES];
}

//
// called when a transaction has been restored and and successfully completed
//
- (void)restoreTransaction:(SKPaymentTransaction *)transaction
{
	NSLog(@"restoreTransaction...%@", transaction.payment.productIdentifier);

    [self finishTransaction:transaction wasSuccessful:YES];
}

//
// called when a transaction has failed
//
- (void)failedTransaction:(SKPaymentTransaction *)transaction
{
	NSLog(@"failedTransaction...%@", transaction.payment.productIdentifier);
    
	if (transaction.error.code != SKErrorPaymentCancelled) {
		// error!
        NSLog(@"Transaction error: %@", transaction.error.localizedDescription);
        [self finishTransaction:transaction wasSuccessful:NO];
    } else {
        // this is fine, the user just cancelled
        [self finishTransaction:transaction wasSuccessful:NO];
    }
}

- (void)paymentQueue:(SKPaymentQueue *)queue restoreCompletedTransactionsFailedWithError:(NSError *)error
{
    NSLog(@"Restore Error: %@", error.localizedDescription);
    inapp_purchase_cb("@restore", 0);
}

- (void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue
{
	NSLog(@"Restore Done");
	inapp_purchase_cb("@restore", 1);
}

////SKProductsRequestDelegate
- (void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response{
	NSLog(@"Products Received");
	
    NSArray *products = response.products;
    proUpgradeProduct = [products count] >= 1 ? [[products firstObject] retain] : nil;
    if (proUpgradeProduct) {
		NSLog(@"Product title: %@" , proUpgradeProduct.localizedTitle);
		NSLog(@"Product description: %@" , proUpgradeProduct.localizedDescription);
		NSLog(@"Product price: %@" , proUpgradeProduct.price);
        NSLog(@"Product id: %@" , proUpgradeProduct.productIdentifier);
	
        NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
        [numberFormatter setFormatterBehavior:NSNumberFormatterBehavior10_4];
        [numberFormatter setNumberStyle:NSNumberFormatterCurrencyStyle];
        [numberFormatter setLocale:proUpgradeProduct.priceLocale];
        NSString *formattedString = [numberFormatter stringFromNumber:proUpgradeProduct.price];
        NSLog(@"Product id: %@" , formattedString);
    }

	for (NSString *invalidProductId in response.invalidProductIdentifiers) {
		NSLog(@"Invalid product id: %@" , invalidProductId);
	}

    startAppPayment();
}
@end

InAppPurchaseManager* observer = nil;

void startShop()
{
	if (!observer) {
		observer = [[InAppPurchaseManager alloc] init];
		[[SKPaymentQueue defaultQueue] addTransactionObserver:observer];
	}
}

void stopShop()
{
	if (observer) {
		// [observer release];
        // observer = nil;
	}
}

void getProducts()
{
	NSLog(@"getProducts Called..");
	NSMutableSet* pids = nil;//[[NSMutableSet alloc] initWithCapacity:32];
    
    NSString* p = @"com.ancientcc.testwin.getcard,com.ancientcc.testwin.tacticslot";
    pids = [NSSet setWithArray:[p componentsSeparatedByString:@","]];
    
    observer = [[InAppPurchaseManager alloc] init];
	[[SKPaymentQueue defaultQueue] addTransactionObserver:observer];
	
    SKProductsRequest* req = [[SKProductsRequest alloc] initWithProductIdentifiers:pids];
	req.delegate = observer;
	[req start];
}

void startAppPayment(const char* identifier)
{
    NSString* id = [[NSString alloc] initWithUTF8String:identifier];
	SKPayment* payment = [SKPayment paymentWithProductIdentifier:id];
	[id release];
	
	// SKMutablePayment* payment = [SKMutablePayment paymentWithProduct:proUpgradeProduct];
    // payment.quantity = 2;
    [[SKPaymentQueue defaultQueue] addPayment:payment];
}

void restoreAppPayment()
{
    [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
}

void get_serialnumber(char* sn)
{
    UIDevice* device = [UIDevice currentDevice];
	
    NSLog(@"get_serialnumber...%@", [device localizedModel]);
    NSLog(@"get_serialnumber...%@", [device model]);
    NSLog(@"get_serialnumber...%@", [device systemVersion]);
    NSLog(@"get_serialnumber...%@", [device systemName]);
    
    strcpy(sn, [[device systemVersion] UTF8String]);
}