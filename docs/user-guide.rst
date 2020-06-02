     
 
.. This work is licensed under a Creative Commons Attribution 4.0 International License. 
.. SPDX-License-Identifier: CC-BY-4.0 
.. 
.. CAUTION: this document is generated from source in doc/src/* 
.. To make changes edit the source and recompile the document. 
.. Do NOT make changes directly to .rst or .md files. 
 
 
============
User's Guide 
============
---------------------
Traffic Steering xAPP
---------------------
 
Introduction 
============

The Traffic Steering Use Case demonstrates intelligent inferences in the Near-RT RIC and E2 interaction in order to execute on the inferences. 

The current Use Case is comprised of three xApps:
* Traffic Steering xApp (this one): Consume A1 Policy Intent, regularly monitor RAN metrics and request prediction for badly performing UEs, and listen for messages that show UE throughput predictions in different cells, in order to make a decision about UE Handover.
* QoE Prediction (QP) xApp: Receive a feature set of metrics for a given UE, and output Throughput predictions on the Serving and any Neighbor cells
* QoE Prediction Driver (QP Driver) xApp: Generate a feature set of metrics to input to QoE Prediction, based on SDL lookups in UE-Metric and Cell-Metric namespaces

A1 Policy
=========

A1 Policy is sent to Traffic Steering xApp to define the Intent which will drive the Traffic Steering behavior.

Policy Type ID is 20008.

Currently, there is only one parameter that can be provided in A1 Policy: threshold

An example Policy follows:
{"threshold" : 5}

This Policy instructs Traffic Steering xApp to monitor current RAN metrics and request a QoE Prediction for any UE whose Serving Cell RSRP is less than 5.

Sending QoE Prediction Request
==============================

Traffic Steering xApp loops repeatedly.  After every sleep, it queries the SDL UE-Metric namespace.  When it identifies a UE whose RSRP is below the threshold, it generates a QoE Prediction message.  The RMR Message Type is 30000.  The following is an example message body:

{"UEPredictionSet" : ["12345"]}

Receiving QoE Prediction
========================

Traffic Steering xApp defines a callback for QoE Prediction received from QP xApp.  The RMR message type is 30002.  The following is an example message body:

{"12345" : { "310-680-200-555001" : [ 2000000 , 1200000 ] , "310-680-200-555002" : [ 800000 , 400000 ] , "310-680-200-555003" : [ 800000 , 400000 ]  } }

This message provides predictions for UE ID 12345.  For its service cell and neighbor cells, it lists an array containing two elements: DL Throughput and UL Throughput predictions.

Traffic Steering xApp checks for the Service Cell ID for UE ID, and determines whether the predicted throughput is higher in a neighbor cell. 

If predicted throughput is higher in a neighbor cell, Traffic Steering logs its intention to send a CONTROL message to do handover.

