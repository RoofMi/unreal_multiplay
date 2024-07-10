// Fill out your copyright notice in the Description page of Project Settings.


#include "UdemyPlatformGameInstance.h"

#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/UserWidget.h"

#include "PlatformTrigger.h"
#include "MenuSystem/MainMenu.h"

UUdemyPlatformGameInstance::UUdemyPlatformGameInstance(const FObjectInitializer& ObjectInitializer)
{
	ConstructorHelpers::FClassFinder<UUserWidget> MenuBPClass(TEXT("/Game/Udemy/WBP_MainMenu"));
	if (MenuBPClass.Class != nullptr)
	{
		MenuClass = MenuBPClass.Class;
	}
	
}

// Play할 때 실행됨
void UUdemyPlatformGameInstance::Init()
{
	
}

void UUdemyPlatformGameInstance::LoadMenu()
{
	Menu = CreateWidget<UMainMenu>(this, MenuClass);

	if (!ensure(Menu != nullptr))
		return;

	Menu->Setup();
	Menu->SetMenuInterface(this);
}

void UUdemyPlatformGameInstance::Host()
{
	if (Menu != nullptr)
		Menu->Teardown();

	UEngine* Engine = GetEngine();

	Engine->AddOnScreenDebugMessage(0, 5, FColor::Green, FString::Printf(TEXT("Hosting!")));

	UWorld* World = GetWorld();

	if (World != nullptr) {
		World->ServerTravel("/Game/ThirdPerson/Maps/ThirdPersonMap?listen");
	}
}

void UUdemyPlatformGameInstance::Join(const FString& Address)
{
	if (Menu != nullptr)
		Menu->Teardown();

	UEngine* Engine = GetEngine();

	Engine->AddOnScreenDebugMessage(0, 5, FColor::Green, FString::Printf(TEXT("Joining % s"), *Address));

	APlayerController* PlayerController = GetFirstLocalPlayerController();

	if (PlayerController != nullptr) {
		PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
	}
}