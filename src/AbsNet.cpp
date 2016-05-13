/*
 * AbsNet.cpp
 *
 *  Created on: 16.02.2011
 *      Author: dgrat
 */
#include <iostream>
#include <cassert>
#include <omp.h>
//own classes
#include "AbsNeuron.h"
#include "AbsLayer.h"
#include "AbsNet.h"
#include "Edge.h"
#include "Common.h"
#include "containers/TrainingSet.h"
#include "containers/ConTable.h"
#include "math/Random.h"
#include "math/Functions.h"


using namespace ANN;


template <class Type>
AbsNet<Type>::AbsNet() //: Importer(this),  Exporter(this)
{
	m_fLearningRate = 0.0f;
	m_fMomentum 	= 0.f;
	m_fWeightDecay 	= 0.f;
	m_pTransfFunction 	= NULL;
	m_pTrainingData = NULL;

	m_pIPLayer 		= NULL;
	m_pOPLayer 		= NULL;

	m_fTypeFlag 	= ANNetUndefined;

	// Init time for rdom numbers
	INIT_TIME
}

template <class Type>
void AbsNet<Type>::CreateNet(const ConTable &Net) {
	std::cout<<"Create AbsNet()"<<std::endl;

	/*
	 * Initialisiere Variablen
	 */
	unsigned int iNmbLayers 	= Net.NrOfLayers;	// zahl der Layer im Netz
	unsigned int iNmbNeurons	= 0;

	unsigned int iDstNeurID 	= 0;
	unsigned int iSrcNeurID 	= 0;
	unsigned int iDstLayerID 	= 0;
	unsigned int iSrcLayerID 	= 0;

	Type fEdgeValue			= 0.f;

	AbsLayer<Type> *pDstLayer 	= NULL;
	AbsLayer<Type> *pSrcLayer 	= NULL;
	AbsNeuron<Type> *pDstNeur 	= NULL;
	AbsNeuron<Type> *pSrcNeur 	= NULL;

	LayerTypeFlag fType 		= 0;

	/*
	 *	Delete existing network in memory
	 */
	EraseAll();
	SetFlag(Net.NetType);

	/*
	 * Create the layers ..
	 */
	std::cout<<"Adding layers ";
	for(unsigned int i = 0; i < iNmbLayers; i++) {
		iNmbNeurons = Net.SizeOfLayer.at(i);
		fType 		= Net.TypeOfLayer.at(i);

		// Create layers
		AddLayer(iNmbNeurons, fType);

		// Set pointers to input and output layers
		if(fType & ANLayerInput) {
			SetIPLayer(i);
		}
		else if(fType & ANLayerOutput) {
			SetOPLayer(i);
		}
		else if(fType & (ANLayerInput | ANLayerOutput) ) {	// Hopfield networks
			SetIPLayer(i);
			SetOPLayer(i);
		}
	}
	std::cout<<".. finished!"<<std::endl;

	/*
	 * Basic information for ~all networks
	 */
	std::cout<<"Adding edges ";
	for(unsigned int i = 0; i < Net.NeurCons.size(); i++) {
		/*
		 * Read settings
		 */
		iDstNeurID 	= Net.NeurCons.at(i).m_iDstNeurID;
		iSrcNeurID 	= Net.NeurCons.at(i).m_iSrcNeurID;
		iDstLayerID = Net.NeurCons.at(i).m_iDstLayerID;
		iSrcLayerID = Net.NeurCons.at(i).m_iSrcLayerID;

		/*
		 * Check whether all settings are usable
		 */
		assert(iDstNeurID 	>= 0);
		assert(iSrcNeurID 	>= 0);
		assert(iDstLayerID 	>= 0);
		assert(iSrcLayerID 	>= 0);
		assert(i < Net.NeurCons.size());

		/*
		 * Create edges and register in neurons
		 */
		fEdgeValue 	= Net.NeurCons.at(i).m_fVal;
		pDstLayer 	= GetLayer(iDstLayerID);
		pSrcLayer 	= GetLayer(iSrcLayerID);
		pDstNeur 	= pDstLayer->GetNeuron(iDstNeurID);
		pSrcNeur 	= pSrcLayer->GetNeuron(iSrcNeurID);

		// Check for NULL pointers
		assert(pDstLayer 	!= NULL);
		assert(pSrcLayer 	!= NULL);
		assert(pDstNeur 	!= NULL);
		assert(pSrcNeur 	!= NULL);

		//Connect neurons with edge
		ANN::Connect<Type>(pSrcNeur, pDstNeur, fEdgeValue, 0.f, true);
	}
	std::cout<<".. finished!"<<std::endl;
}

template <class Type>
AbsNet<Type>::~AbsNet() {
	EraseAll();
}

template <class Type>
void AbsNet<Type>::SetFlag(const NetTypeFlag &fType) {
	m_fTypeFlag = fType;
}

template <class Type>
void AbsNet<Type>::AddFlag(const NetTypeFlag &fType) {
	if( !(m_fTypeFlag & fType) )
		m_fTypeFlag |= fType;
}

template <class Type>
LayerTypeFlag AbsNet<Type>::GetFlag() const {
	return m_fTypeFlag;
}

template <class Type>
void AbsNet<Type>::EraseAll() {
	#pragma omp parallel for
	for(int i = 0; i < static_cast<int>( m_lLayers.size() ); i++) {
		m_lLayers.at(i)->EraseAll();
	}
	m_lLayers.clear();
}

template <class Type>
std::vector<Type> AbsNet<Type>::TrainFromData(const unsigned int &iCycles, const Type &fTolerance, const bool &bBreak, Type &fProgress) {
	std::vector<Type> pErrors;

	if(m_pTrainingData == NULL)
		return pErrors;

	Type fCurError 	= 0.f;
	int iProgCount 		= 1;

	for(unsigned int j = 0; j < iCycles; j++) {
		/*
		 * Output for progress bar
		 */
		fProgress = (Type)(j+1)/(Type)iCycles*100.f;
		if(iCycles >= 10) {
			if(((j+1) / (iCycles/10)) == iProgCount && (j+1) % (iCycles/10) == 0) {
				std::cout << "Training progress: " << iProgCount*10.f << "%" << std::endl;
				iProgCount++;
			}
		}
		else {
			std::cout << "Training progress: " << fProgress << "%" << std::endl;
		}

		/*
		 * Break if error is beyond bias
		 */
		if(fCurError < fTolerance && j > 0 || bBreak == true) {
			return pErrors;
		}

		/*
		 * Save current error in a std::vector
		 */
		fCurError 	= 0.f;
		for( unsigned int i = 0; i < m_pTrainingData->GetNrElements(); i++ ) {
			SetInput(m_pTrainingData->GetInput(i) );
			fCurError += SetOutput(m_pTrainingData->GetOutput(i) );
			PropagateBW();
		}
		pErrors.push_back(fCurError);
	}
	return pErrors;
}

template <class Type>
void AbsNet<Type>::AddLayer(AbsLayer<Type> *pLayer) {
	m_lLayers.push_back(pLayer);
	pLayer->SetID( m_lLayers.size()-1 );
}

template <class Type>
std::vector<AbsLayer<Type>*> AbsNet<Type>::GetLayers() const {
	return m_lLayers;
}

template <class Type>
AbsLayer<Type>* AbsNet<Type>::GetLayer(const unsigned int &iID) const {
	assert( m_lLayers.at(iID) != NULL );
	assert( iID >= 0 );
	assert( iID < m_lLayers.size() );

	return m_lLayers.at(iID);
}

template <class Type>
void AbsNet<Type>::SetInput(const std::vector<Type> &inputArray) {
	assert( m_pIPLayer != NULL );
	assert( inputArray.size() <= m_pIPLayer->GetNeurons().size() );

	AbsNeuron<Type> *pCurNeuron;
	for(int i = 0; i < static_cast<int>( m_pIPLayer->GetNeurons().size() ); i++) {
		pCurNeuron = m_pIPLayer->GetNeuron(i);
		pCurNeuron->SetValue(inputArray[i]);
	}
}

template <class Type>
void AbsNet<Type>::SetInput(const std::vector<Type> &inputArray, const unsigned int &layerID) {
//	assert( m_lLayers[layerID]->GetFlag() & LayerInput );
	assert( layerID < m_lLayers.size() );
	assert( inputArray.size() <= m_lLayers[layerID]->GetNeurons().size() );

	AbsNeuron<Type> *pCurNeuron;
	for(int i = 0; i < static_cast<int>( m_lLayers[layerID]->GetNeurons().size() ); i++) {
		pCurNeuron = m_lLayers[layerID]->GetNeuron(i);
		pCurNeuron->SetValue(inputArray[i]);
	}
}

template <class Type>
void AbsNet<Type>::SetInput(Type *inputArray, const unsigned int &size, const unsigned int &layerID) {
//	assert( m_lLayers[layerID]->GetFlag() & LayerInput );
	assert( layerID < m_lLayers.size() );
	assert( size <= m_lLayers[layerID]->GetNeurons().size() );

	AbsNeuron<Type> *pCurNeuron;
	for(int i = 0; i < static_cast<int>( m_lLayers[layerID]->GetNeurons().size() ); i++) {
		pCurNeuron = m_lLayers[layerID]->GetNeuron(i);
		pCurNeuron->SetValue(inputArray[i]);
	}
}

template <class Type>
Type AbsNet<Type>::SetOutput(const std::vector<Type> &outputArray) {
	assert( m_pOPLayer != NULL );
	assert( outputArray.size() == m_pOPLayer->GetNeurons().size() );

	PropagateFW();

	Type fError 		= 0.f;
	Type fCurError 	= 0.f;
	AbsNeuron<Type> *pCurNeuron = NULL;
	for(unsigned int i = 0; i < m_pOPLayer->GetNeurons().size(); i++) {
		pCurNeuron = m_pOPLayer->GetNeuron(i);
		fCurError = outputArray[i] - pCurNeuron->GetValue();
		fError += pow( fCurError, 2 ) / 2.f;
		pCurNeuron->SetErrorDelta(fCurError);
	}
	return fError;
}

template <class Type>
Type AbsNet<Type>::SetOutput(const std::vector<Type> &outputArray, const unsigned int &layerID) {
//	assert( m_lLayers[layerID]->GetFlag() & LayerOutput );
	assert( layerID < m_lLayers.size() );
	assert( outputArray.size() == m_lLayers[layerID]->GetNeurons().size() );

	PropagateFW();

	Type fError 		= 0.f;
	Type fCurError 	= 0.f;
	AbsNeuron<Type> *pCurNeuron = NULL;
	for(unsigned int i = 0; i < m_lLayers[layerID]->GetNeurons().size(); i++) {
		pCurNeuron = m_lLayers[layerID]->GetNeuron(i);
		fCurError = outputArray[i] - pCurNeuron->GetValue();
		fError += pow( fCurError, 2 ) / 2.f;
		pCurNeuron->SetErrorDelta(fCurError);
	}
	return fError;
}

template <class Type>
Type AbsNet<Type>::SetOutput(Type *outputArray, const unsigned int &size, const unsigned int &layerID) {
	assert( layerID < m_lLayers.size() );
	assert( size == m_lLayers[layerID]->GetNeurons().size() );

	PropagateFW();

	Type fError 		= 0.f;
	Type fCurError 	= 0.f;
	AbsNeuron<Type> *pCurNeuron = NULL;
	for(unsigned int i = 0; i < m_lLayers[layerID]->GetNeurons().size(); i++) {
		pCurNeuron = m_lLayers[layerID]->GetNeuron(i);
		fCurError = outputArray[i] - pCurNeuron->GetValue();
		fError += pow( fCurError, 2 ) / 2.f;
		pCurNeuron->SetErrorDelta(fCurError);
	}
	return fError;
}

template <class Type>
void AbsNet<Type>::SetTrainingSet(const TrainingSet<Type> *pData) {
	assert(pData);

	if( pData != NULL ) {
		m_pTrainingData = const_cast<TrainingSet<Type> *>(pData);
	}
}

template <class Type>
void AbsNet<Type>::SetTrainingSet(const TrainingSet<Type> &pData) {
	m_pTrainingData = (TrainingSet<Type>*)&pData;
}

template <class Type>
TrainingSet<Type> *AbsNet<Type>::GetTrainingSet() const {
	assert(m_pTrainingData);

	return m_pTrainingData;
}

template <class Type>
AbsLayer<Type> *AbsNet<Type>::GetIPLayer() const {
	assert(m_pIPLayer);

	return m_pIPLayer;
}

template <class Type>
AbsLayer<Type> *AbsNet<Type>::GetOPLayer() const {
	assert(m_pOPLayer);

	return m_pOPLayer;
}
/*
const AbsLayer *AbsNet<Type>::GetIPLayer() const {
	assert(m_pIPLayer);

	return m_pIPLayer;
}

const AbsLayer *AbsNet<Type>::GetOPLayer() const {
	assert(m_pOPLayer);

	return m_pOPLayer;
}
*/

template <class Type>
void AbsNet<Type>::SetIPLayer(const unsigned int iID) {
	assert (iID >= 0);
	assert (iID < GetLayers().size() );

	m_pIPLayer = GetLayer(iID);
}

template <class Type>
void AbsNet<Type>::SetOPLayer(const unsigned int iID) {
	assert (iID >= 0);
	assert (iID < GetLayers().size() );

	m_pOPLayer = GetLayer(iID);
}

template <class Type>
std::vector<Type> AbsNet<Type>::GetOutput() {
	assert( m_pOPLayer != NULL );

	std::vector<Type> vResult;
	for(unsigned int i = 0; i < GetOPLayer()->GetNeurons().size(); i++) {
		AbsNeuron<Type> *pCurNeuron = GetOPLayer()->GetNeuron(i);
		vResult.push_back(pCurNeuron->GetValue() );
	}

	return vResult;
}

template <class Type>
void AbsNet<Type>::SetTransfFunction(const TransfFunction *pFunction) {
	assert( pFunction != 0 );

	m_pTransfFunction = const_cast<TransfFunction *>(pFunction);
	for(unsigned int i = 0; i < m_lLayers.size(); i++) {
		GetLayer(i)->SetNetFunction(m_pTransfFunction);
	}
}

template <class Type>
void AbsNet<Type>::SetTransfFunction(const TransfFunction &pFunction) {
	m_pTransfFunction = const_cast<TransfFunction *>(&pFunction);
	for(unsigned int i = 0; i < m_lLayers.size(); i++) {
		GetLayer(i)->SetNetFunction(m_pTransfFunction);
	}
}

template <class Type>
TransfFunction *AbsNet<Type>::GetTransfFunction() {
	return m_pTransfFunction;
}

template <class Type>
void AbsNet<Type>::ExpToFS(std::string path) {
	int iBZ2Error;
	NetTypeFlag fNetType 		= GetFlag();
	unsigned int iNmbOfLayers 	= GetLayers().size();

	FILE 	*fout = fopen(path.c_str(), "wb");
	BZFILE	*bz2out;
	bz2out = BZ2_bzWriteOpen(&iBZ2Error, fout, 9, 0, 0);

	if (iBZ2Error != BZ_OK) {
		std::cout<<"return: "<<"SaveNetwork()"<<std::endl;
		return;
	}
	std::cout<<"Save network.."<<std::endl;
	BZ2_bzWrite( &iBZ2Error, bz2out, &fNetType, sizeof(int) );
	BZ2_bzWrite( &iBZ2Error, bz2out, &iNmbOfLayers, sizeof(int) );

	for(unsigned int i = 0; i < iNmbOfLayers; i++) {
		GetLayer(i)->ExpToFS(bz2out, iBZ2Error);
	}

	bool bTrainingSet = false;
	if(m_pTrainingData) {
		bTrainingSet = true;
		BZ2_bzWrite( &iBZ2Error, bz2out, &bTrainingSet, sizeof(bool) );
		m_pTrainingData->ExpToFS(bz2out, iBZ2Error);
	}

	BZ2_bzWriteClose ( &iBZ2Error, bz2out, 0, NULL, NULL );
	fclose( fout );
}

template <class Type>
void AbsNet<Type>::ImpFromFS(std::string path) {
	int iBZ2Error;
	ConTable Table;
	NetTypeFlag fNetType 		= 0;
	unsigned int iNmbOfLayers 	= 0;

	FILE *fin = fopen(path.c_str(), "rb");
	BZFILE* bz2in;
	bz2in = BZ2_bzReadOpen(&iBZ2Error, fin, 0, 0, NULL, 0);

	if (iBZ2Error != BZ_OK) {
		std::cout<<"return: "<<"LoadNetwork()"<<std::endl;
		return;
	}

	std::cout<<"Load network.."<<std::endl;
	BZ2_bzRead( &iBZ2Error, bz2in, &fNetType, sizeof(int) );
	Table.NetType 		= fNetType;
	BZ2_bzRead( &iBZ2Error, bz2in, &iNmbOfLayers, sizeof(int) );
	Table.NrOfLayers 	= iNmbOfLayers;

	for(unsigned int i = 0; i < iNmbOfLayers; i++) {
		AddLayer(0, 0); // Create dummy layer; more layers than needed don't disturb, but are necessary if using empty nets

		GetLayer(i)->ImpFromFS(bz2in, iBZ2Error, Table);
	}

	CreateNet( Table );

	bool bTrainingSet = false;
	BZ2_bzRead( &iBZ2Error, bz2in, &bTrainingSet, sizeof(bool) );
	if(bTrainingSet) {
		m_pTrainingData = new ANN::TrainingSet<Type>;
		m_pTrainingData->ImpFromFS(bz2in, iBZ2Error);
	}

	BZ2_bzReadClose ( &iBZ2Error, bz2in );
	fclose(fin);
}


template class AbsNet<float>;
template class AbsNet<double>;
template class AbsNet<long double>;
template class AbsNet<short>;
template class AbsNet<int>;
template class AbsNet<long>;
template class AbsNet<long long>;