

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

The current Use Case is comprised of five xApps:

* KPI Monitoring xApp: Gathers the radio and system Key Performance Indicators (KPI) metrics from E2 Nodes and stores them in the Shared Data Layer (SDL).
* Anomaly Detection (AD) xApp: Fetches UE data regularly from SDL, monitors UE metrics and sends the anomalous UEs to Traffic Steering xApp.
* Traffic Steering xApp (*this one*): Consumes A1 Policy Intent, listens for badly performing UEs, sends prediction requests to QP Driver, and listens for messages that show UE throughput predictions in different cells to make a decision about UE Handover.
* QoE Prediction Driver (QP Driver) xApp: Generates a feature set of metrics to input to QoE Prediction, based on SDL lookups in UE-Metric and Cell-Metric namespaces.
* QoE Prediction (QP) xApp: Receives a feature set of metrics for a given UE, and output Throughput predictions on the Serving and any Neighbor cells to Traffic Steering xApp.

A1 Policy
=========

A1 Policy is sent to Traffic Steering xApp to define the Intent which will drive the Traffic Steering behavior.

Policy Type ID is 20008.

Currently, there is only one parameter that can be provided in A1 Policy: *threshold*

An example Policy follows:

.. code-block::

    { "threshold": 5 }

.. FIXME Is the "Serving Cell RSRP" related to "Degradation" in AD message

This Policy instructs Traffic Steering xApp to request a QoE Prediction for any UE whose Serving Cell RSRP is less than 5.
Traffic Steering logs each A1 Policy update.

Receiving Anomaly Detection
===========================

Traffic Sterring xApp defines a callback to listen to Anomaly Detection messages received from AD xApp. The RMR message type is 30003.
The following is an example message body:

.. code-block::

    [
        {
            "du-id":1010,
            "ue-id":"Train passenger 2",
            "measTimeStampRf":1620835470108,
            "Degradation":"RSRP RSSINR"
        }
    ]

.. ``[{"du-id": 1010, "ue-id": "Train passenger 2", "measTimeStampRf": 1620835470108, "Degradation": "RSRP RSSINR"}]``

Sending QoE Prediction Request
==============================

Traffic Steering listens for badly performing UEs. When it identifies a UE whose RSRP is below the threshold, it generates
a QoE Prediction Request message and sends it to the QP Driver xApp. The RMR Message Type is 30000.
The following is an example message body:

.. {"UEPredictionSet" : ["12345"]}

.. code-block::

    { "UEPredictionSet": ["Train passenger 2"] }

The current version of Traffic Steering xApp does not (yet) consider the A1 policy to generate QoE prediction requests.
Each Anomaly Detection message received from AD xApp, results in a QoE Prediction Request to QP Driver xApp.

Receiving QoE Prediction
========================

Traffic Steering xApp defines a callback for QoE Prediction received from QP xApp.  The RMR message type is 30002.  The following is an example message body:

.. {"12345" : { "310-680-200-555001" : [ 2000000 , 1200000 ] , "310-680-200-555002" : [ 800000 , 400000 ] , "310-680-200-555003" : [ 800000 , 400000 ]  } }

.. code-block::

    {
        "Train passenger 2":{
            "310-680-200-555001":[2000000, 1200000],
            "310-680-200-555002":[1000000, 4000000],
            "310-680-200-555003":[5000000, 4000000]
        }
    }

This message provides predictions for UE ID "Train passenger 2".  For its service cell and neighbor cells, it lists an array containing two elements: DL Throughput and UL Throughput predictions.

Traffic Steering xApp checks for the Service Cell ID for UE ID, and determines whether the predicted throughput is higher in a neighbor cell.
The first cell in this prediction message is assumed to be the serving cell.

If predicted throughput is higher in a neighbor cell, Traffic Steering sends a CONTROL message through a REST call to E2 SIM. This message requests to hand-off the corresponding UE, and an example of its payload is as follows:

.. code-block::

    {
        "command": "HandOff",
        "seqNo": 1,
        "ue": "Train passenger 2",
        "fromCell": "310-680-200-555001",
        "toCell": "310-680-200-555003",
        "timestamp": "Sat May 22 10:35:33 2021",
        "reason": "Hand-Off Control Request from TS xApp",
        "ttl": 10
    }

Traffic Steering also logs the REST response, which shows whether or not the control operation has succeeded.
