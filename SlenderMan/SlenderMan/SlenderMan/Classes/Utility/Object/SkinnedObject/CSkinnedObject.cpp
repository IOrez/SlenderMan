#include "CSkinnedObject.h"
#include "../../../Function/GlobalFunction.h"
#include "../../Base/CDirect3DApplication.h"
#include "../../System/AnimationSystem/AnimationSystem_CAllocateHierarchy.h"
#include "../../System/AnimationSystem/AnimationSystem_CAnimationController.h"
#include "../../Object/LightObject/CLightObject.h"
#include "../../Object/CameraObject/CCameraObject.h"
#include "../../Manager/CDeviceManager.h"
#include "../../Manager/CResourceManager.h"
#include "../../Manager/CTimeManager.h"


CSkinnedObject::CSkinnedObject(const STParameters & a_rstParameters)
	:
	m_stParameters(a_rstParameters)
{
	if (a_rstParameters.m_oMeshFilepath.rfind("/") != std::string::npos) {
		m_oBasepath = a_rstParameters.m_oMeshFilepath.substr(0,
			a_rstParameters.m_oMeshFilepath.rfind("/") + 1);
	}

	m_pEffect = GET_EFFECT(a_rstParameters.m_oEffectFilepath);
	m_pMesh = this->createSkinnedMeshFromX(a_rstParameters.m_oMeshFilepath);

	auto stBoundingBox = CreateBoundingBox(m_pMesh);
	auto stBoundingSphere = CreateBoundingSphere(m_pMesh);

	this->setBoundingBox(stBoundingBox);
	this->setBoundingSphere(stBoundingSphere);
}

CSkinnedObject::~CSkinnedObject(void)
{
	SAFE_DELETE(m_pAnimationController);
}

std::vector<std::string> CSkinnedObject::getAnimationNameList(void)
{
	return m_pAnimationController->getAnimationNameList();
}

void CSkinnedObject::setTimeScale(float a_fTimeScale)
{
	m_pAnimationController->setTimeScale(a_fTimeScale);
}

void CSkinnedObject::update(void)
{
	CRenderObject::update();
	m_pAnimationController->update();

	D3DXMATRIXA16 stWorldMatrix = this->getFinalWorldMatrix();
	this->updateBoneMatrix(m_pstRootBone, stWorldMatrix);
}

void CSkinnedObject::playAnimation(const std::string & a_rAnimationName, bool a_bIsLoop)
{
	m_pAnimationController->playAnimation(a_rAnimationName, a_bIsLoop);
}

void CSkinnedObject::stopAnimation(void)
{
	m_pAnimationController->stopAnimation();
}

void CSkinnedObject::preDraw(void)
{
	CRenderObject::preDraw();
	GET_DEVICE()->SetRenderState(D3DRS_LIGHTING, false);
}

void CSkinnedObject::doDraw(void)
{
	CRenderObject::doDraw();
	D3DXMATRIXA16 stViewMatrix = m_stParameters.m_pCameraObj->getViewMatrix();
	D3DXMATRIXA16 stProjectionMatrix = m_stParameters.m_pCameraObj->getProjectionMatrix();

	m_pEffect->SetMatrix("g_stViewMatrix", &stViewMatrix);
	m_pEffect->SetMatrix("g_stProjectionMatrix", &stProjectionMatrix);

	D3DXVECTOR4 stViewPosition(m_stParameters.m_pCameraObj->getPosition(), 1.0f);
	D3DXVECTOR4 stLightDirection(m_stParameters.m_pDirectionalLight->getForwardDirection(), 0.0f);

	m_pEffect->SetVector("g_stViewPosition", &stViewPosition);
	m_pEffect->SetVector("g_stLightDirection", &stLightDirection);


	this->drawBone(m_pstRootBone);
}

void CSkinnedObject::postDraw(void)
{
	CRenderObject::postDraw();
	GET_DEVICE()->SetRenderState(D3DRS_LIGHTING, true);

}

void CSkinnedObject::updateBoneMatrix(LPD3DXFRAME a_pstFrame, const D3DXMATRIXA16 & a_rstMatrix)
{
	auto pstBone = (CAllocateHierarchy::STBone *)a_pstFrame;
	pstBone->m_stCombineMatrix = pstBone->TransformationMatrix * a_rstMatrix;

	// ???? ?? ?????? ???? ????
	if (pstBone->pFrameSibling != nullptr) {
		this->updateBoneMatrix(pstBone->pFrameSibling, a_rstMatrix);
	}

	// ???? ?? ?????? ???? ????
	if (pstBone->pFrameFirstChild != nullptr) {
		this->updateBoneMatrix(pstBone->pFrameFirstChild, pstBone->m_stCombineMatrix);
	}
}

void CSkinnedObject::drawBone(LPD3DXFRAME a_pstFrame)
{
	auto pstMeshContainer = a_pstFrame->pMeshContainer;

	while (pstMeshContainer != nullptr) {
		this->drawMeshContainer(a_pstFrame, pstMeshContainer);
		pstMeshContainer = pstMeshContainer->pNextMeshContainer;
	}

	// ???? ?? ?????? ???? ????
	if (a_pstFrame->pFrameSibling != nullptr) {
		this->drawBone(a_pstFrame->pFrameSibling);
	}

	// ???? ?? ?????? ???? ????
	if (a_pstFrame->pFrameFirstChild != nullptr) {
		this->drawBone(a_pstFrame->pFrameFirstChild);
	}
}

void CSkinnedObject::drawMeshContainer(LPD3DXFRAME a_pstFrame, LPD3DXMESHCONTAINER a_pstMeshContainer)
{
	if (a_pstMeshContainer->pSkinInfo != nullptr) {

		auto pstMeshContainer = (CAllocateHierarchy::STMeshContainer *)a_pstMeshContainer;

		for (int i = 0; i < pstMeshContainer->m_oBoneCombinationList.size(); ++i) {
			int nNumBlends = 0;
			auto stBoneCombination = pstMeshContainer->m_oBoneCombinationList[i];

			// ???? ?????? ???? ?????? ????????
			for (int j = 0; j < pstMeshContainer->m_nNumBlends; ++j) {
				if (stBoneCombination.BoneId[j] != UINT_MAX) {
					nNumBlends = j;
				}
			}

			// ???? ?????? ????????
			// {
			D3DXMATRIXA16 astWorldMatrices[4];
			int nSize = sizeof(astWorldMatrices) / sizeof(astWorldMatrices[0]);

			for (int j = 0; j < nSize; ++j) {
				D3DXMatrixIdentity(astWorldMatrices + j);
			}
			// }

			for (int j = 0; j < pstMeshContainer->m_nNumBlends; ++j) {
				int nBoneID = stBoneCombination.BoneId[j];

				if (nBoneID != UINT_MAX) {
					auto pstBone = pstMeshContainer->m_oBoneList[nBoneID];
					D3DXMATRIXA16 stBoneMatrix = pstMeshContainer->m_oBoneMatrixList[nBoneID];

					astWorldMatrices[j] = stBoneMatrix * pstBone->m_stCombineMatrix;
				}
			}

			m_pEffect->SetInt("g_nNumBlends", nNumBlends);
			m_pEffect->SetMatrixArray("g_stWorldMatrices", astWorldMatrices, 4);

			int nAttributeID = stBoneCombination.AttribId;
			m_pEffect->SetTexture("g_pTexture", pstMeshContainer->m_oTextureList[nAttributeID]);

			RunEffectLoop(m_pEffect, "DefaultSkinnedMesh", [=](int a_nPassNumber) -> void {
				pstMeshContainer->m_pSkinnedMesh->DrawSubset(i);
			});
		}
	}
}

void CSkinnedObject::setupBone(LPD3DXFRAME a_pstFrame)
{
	if (a_pstFrame->pMeshContainer != nullptr) {
		this->setupBoneOnMeshContainer(a_pstFrame, a_pstFrame->pMeshContainer);
	}

	// ???? ?? ?????? ???? ????
	if (a_pstFrame->pFrameSibling != nullptr) {
		this->setupBone(a_pstFrame->pFrameSibling);
	}

	// ???? ?? ?????? ???? ????
	if (a_pstFrame->pFrameFirstChild != nullptr) {
		this->setupBone(a_pstFrame->pFrameFirstChild);
	}
}

void CSkinnedObject::setupBoneOnMeshContainer(LPD3DXFRAME a_pstFrame, LPD3DXMESHCONTAINER a_pstMeshContainer)
{
	auto pstMeshContainer = (CAllocateHierarchy::STMeshContainer *)a_pstMeshContainer;

	if (pstMeshContainer->pSkinInfo != nullptr) {
		int nNumBones = pstMeshContainer->pSkinInfo->GetNumBones();

		for (int i = 0; i < nNumBones; ++i) {
			/*
			GetBoneName ?????? ???? ???????? ???????? ???? ???? ??????
			???????? ?????? ????.
			*/
			auto pszBoneName = pstMeshContainer->pSkinInfo->GetBoneName(i);

			/*
			D3DXFrameFind ?????? ???? ???? ???? ?????? ???????? ???? ????
			???????? ?????? ????.
			*/
			auto pstBone = D3DXFrameFind(m_pstRootBone, pszBoneName);
			pstMeshContainer->m_oBoneList.push_back((CAllocateHierarchy::STBone *)pstBone);
		}
	}
}

LPD3DXMESH CSkinnedObject::createSkinnedMeshFromX(const std::string & a_rFilepath)
{
	CAllocateHierarchy::STParameters stParameters = {
		m_oBasepath,
		std::bind(&CSkinnedObject::createSkinnedMesh, this, std::placeholders::_1, std::placeholders::_2)
	};

	CAllocateHierarchy oAllocateHierarchy(stParameters);
	LPD3DXANIMATIONCONTROLLER pAnimationController = nullptr;

	// ?????? ????????
	D3DXLoadMeshHierarchyFromXA(a_rFilepath.c_str(),
		D3DXMESH_MANAGED,
		GET_DEVICE(),
		&oAllocateHierarchy,
		NULL,
		(LPD3DXFRAME *)&m_pstRootBone,			// ?????? ???????? ?? ???? ?????? ??
		&pAnimationController);					// ???????????? ???????? ???? ????????

	// ???? ????????
	this->setupBone(m_pstRootBone);

	// ?????????? ?????????? ????????
	m_pAnimationController = new CAnimationController(pAnimationController);

	/*
	?????? ?????? ?????? ?????? MeshContainer ?? N ?? ?????? ???? ?? ?? ???? ??????
	???? MeshContainer ?????? ???? ?????? ?????? ?????? ???? ???? ?????? ????????.

	(??, ?????????? ???? ?????? ???????? MeshContainer ?? ?????? ?????? ??????
	????????.)
	*/
	return m_oMeshContainerList.front()->m_pSkinnedMesh;
}

LPD3DXMESH CSkinnedObject::createSkinnedMesh(LPD3DXMESHCONTAINER a_pstMeshContainer, int a_nMeshContainerNumber)
{
	auto pstMeshContainer = (CAllocateHierarchy::STMeshContainer *)a_pstMeshContainer;

	DWORD nNumBlends = 0;
	DWORD nNumBoneCombinations = 0;

	LPD3DXMESH pMesh = nullptr;
	LPD3DXBUFFER pBoneCombinationBuffer = nullptr;

	// ?????? ?????? ????????
	pstMeshContainer->pSkinInfo->ConvertToBlendedMesh(pstMeshContainer->MeshData.pMesh,
		pstMeshContainer->MeshData.pMesh->GetOptions() | D3DXMESHOPT_VERTEXCACHE,
		pstMeshContainer->pAdjacency,
		pstMeshContainer->pAdjacency,
		NULL,
		NULL,
		&nNumBlends,
		&nNumBoneCombinations,
		&pBoneCombinationBuffer,
		&pMesh);

	// ???? ?????? ????????
	// {
	D3DVERTEXELEMENT9 astElements[] = {
		0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0,
		0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0,
		0, 20, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0,
		0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0,
		0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0,
		0, 56, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0,
		D3DDECL_END()
	};
		/*
		?????? ???????????? ???????? ???????? ???? ?????? ?????? ???? ?????? ??????
		???????? BLENDWEIGHT ???????? ?????? ??????????????????.
		*/

	auto pOriginMesh = pMesh;

	// ???? ?????? ????????
	pOriginMesh->CloneMesh(pOriginMesh->GetOptions(),
		astElements,
		GET_DEVICE(),
		&pMesh);

	// ???? ?????? ????????
	D3DXComputeNormals(pMesh, pstMeshContainer->pAdjacency);

	// ????, ?????? ?????? ????????
	D3DXComputeTangent(pMesh, 0, 0, 0, TRUE, pstMeshContainer->pAdjacency);
	// }

	// ?????? ?????? ????????
	// {
	pstMeshContainer->m_nNumBlends = nNumBlends;
	auto pstBoneCombinations = (LPD3DXBONECOMBINATION)pBoneCombinationBuffer->GetBufferPointer();

	// ?? ???? ?????? ????????
	for (int i = 0; i < nNumBoneCombinations; ++i) {
		D3DXBONECOMBINATION stBoneCombination = pstBoneCombinations[i];

		auto stCopiedBoneCombination = stBoneCombination;
		stCopiedBoneCombination.BoneId = (DWORD *)malloc(sizeof(DWORD) * nNumBlends);

		CopyMemory(stCopiedBoneCombination.BoneId, stBoneCombination.BoneId, sizeof(DWORD) * nNumBlends);
		pstMeshContainer->m_oBoneCombinationList.push_back(stCopiedBoneCombination);
	}
	// }

	SAFE_RELEASE(pOriginMesh);
	SAFE_RELEASE(pBoneCombinationBuffer);

	m_oMeshContainerList.push_back(pstMeshContainer);
	return pMesh;
}
