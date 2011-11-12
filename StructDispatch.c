#include "StructDispatch.h"

HRESULT STDMETHODCALLTYPE struct_GetTypeInfoCount(COMObject * pThis, UINT * pCount)
{
	(*pCount)=0;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE struct_GetTypeInfo(COMObject * pThis, UINT itinfo, LCID lcid, ITypeInfo ** pTypeInfo)
{
	return 0;
}

HRESULT STDMETHODCALLTYPE struct_GetIDsOfNames(COMObject * pThis, REFIID riid, LPOLESTR * rgszNames, UINT cNames, LCID lcid, DISPID * rgdispid)
{

	return S_OK;
}

HRESULT STDMETHODCALLTYPE struct_Invoke(COMObject * pThis, DISPID dispid, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *params, VARIANT *result, EXCEPINFO *pexcepinfo, UINT *puArgErr)
{
	if(wFlags==DISPATCH_PROPERTYGET)
	{
		return S_OK;
	}
	else if(wFlags==DISPATCH_PROPERTYPUT)
	{
		return S_OK;
	}
	else
		return DISP_E_MEMBERNOTFOUND;
}