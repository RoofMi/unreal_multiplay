// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ServerRow.generated.h"

/**
 * 
 */
UCLASS()
class UDEMYPROJECT_API UServerRow : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Meta = (BindWidget))
	class UTextBlock* ServerName;

	void Setup(class UMainMenu* InParent, uint32 InIndex);

private:
	UPROPERTY()
	class UMainMenu* Parent;

	uint32 Index;

	UPROPERTY(Meta = (BindWidget))
	class UButton* RowButton;

	UFUNCTION()
	void OnClicked();
};
