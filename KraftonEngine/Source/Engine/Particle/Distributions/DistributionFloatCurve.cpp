#include "DistributionFloatCurve.h"

#include "Serialization/Archive.h"

#include <algorithm>

namespace
{
	static FArchive& SerializeParticleCurveKey(FArchive& Ar, FCurveKey& Key)
	{
		Ar << Key.Time;
		Ar << Key.Value;

		int32 InterpMode = static_cast<int32>(Key.InterpMode);
		Ar << InterpMode;
		if (Ar.IsLoading())
		{
			Key.InterpMode = static_cast<ECurveInterpMode>(InterpMode);
		}

		int32 TangentMode = static_cast<int32>(Key.TangentMode);
		Ar << TangentMode;
		if (Ar.IsLoading())
		{
			Key.TangentMode = static_cast<ECurveTangentMode>(TangentMode);
		}

		Ar << Key.ArriveTangent;
		Ar << Key.LeaveTangent;
		return Ar;
	}

	static void SerializeFloatCurve(FArchive& Ar, FFloatCurve& Curve)
	{
		Ar << Curve.DefaultValue;

		int32 PreExtrap = static_cast<int32>(Curve.PreExtrapMode);
		int32 PostExtrap = static_cast<int32>(Curve.PostExtrapMode);
		Ar << PreExtrap;
		Ar << PostExtrap;

		if (Ar.IsLoading())
		{
			Curve.PreExtrapMode = static_cast<ECurveExtrapMode>(PreExtrap);
			Curve.PostExtrapMode = static_cast<ECurveExtrapMode>(PostExtrap);
		}

		int32 NumKeys = static_cast<int32>(Curve.Keys.size());
		Ar << NumKeys;

		if (Ar.IsLoading())
		{
			Curve.Keys.clear();
			Curve.Keys.resize((std::max)(0, NumKeys));
		}

		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			SerializeParticleCurveKey(Ar, Curve.Keys[KeyIndex]);
		}

		if (Ar.IsLoading())
		{
			Curve.SortKeys();
		}
	}
}

UDistributionFloatCurve::UDistributionFloatCurve()
{
	SetLinear(0.0f, 0.0f, 1.0f, 1.0f);
}

float UDistributionFloatCurve::GetValue(float Time, UObject* Data) const
{
	(void)Data;
	return Curve.Evaluate(Time);
}

void UDistributionFloatCurve::GetInRange(float& OutMin, float& OutMax) const
{
	if (Curve.Keys.empty())
	{
		OutMin = 0.0f;
		OutMax = 1.0f;
		return;
	}

	OutMin = Curve.Keys.front().Time;
	OutMax = Curve.Keys.front().Time;
	for (const FCurveKey& Key : Curve.Keys)
	{
		OutMin = (std::min)(OutMin, Key.Time);
		OutMax = (std::max)(OutMax, Key.Time);
	}
}

void UDistributionFloatCurve::GetOutRange(float& OutMin, float& OutMax) const
{
	if (Curve.Keys.empty())
	{
		OutMin = Curve.DefaultValue;
		OutMax = Curve.DefaultValue;
		return;
	}

	OutMin = Curve.Keys.front().Value;
	OutMax = Curve.Keys.front().Value;
	for (const FCurveKey& Key : Curve.Keys)
	{
		OutMin = (std::min)(OutMin, Key.Value);
		OutMax = (std::max)(OutMax, Key.Value);
	}
}

void UDistributionFloatCurve::SetConstant(float Value)
{
	Curve.Reset();
	Curve.DefaultValue = Value;
	Curve.AddKey(0.0f, Value, ECurveInterpMode::Linear);
	Curve.AddKey(1.0f, Value, ECurveInterpMode::Linear);
	Curve.SortKeys();
}

void UDistributionFloatCurve::SetLinear(float StartTime, float StartValue, float EndTime, float EndValue)
{
	Curve.Reset();
	Curve.DefaultValue = StartValue;
	Curve.AddKey(StartTime, StartValue, ECurveInterpMode::Linear);
	Curve.AddKey(EndTime, EndValue, ECurveInterpMode::Linear);
	Curve.SortKeys();
	Curve.AutoSetTangents();
}

void UDistributionFloatCurve::Serialize(FArchive& Ar)
{
	SerializeFloatCurve(Ar, Curve);
}
