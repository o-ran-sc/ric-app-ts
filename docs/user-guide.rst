

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
* Traffic Steering xApp (*this one*): Consumes A1 Policy Intent, listens for badly performing UEs, sends prediction requests to QP xApp, and listens for messages from QP that show UE throughput predictions in different cells to make decisions about UE Handover.
* QoE Prediction (QP) xApp: Generates a feature set of metrics based on SDL lookups in UE-Metric and Cell-Metric namespaces for a given UE, and outputs Throughput predictions on the Serving and any Neighbor cells to the Traffic Steering xApp.
* RAN Control (RC) xApp: Provides basic implementation of spec compliant E2-SM RC to send RIC Control Request messages to RAN/E2 Nodes.


A1 Policy
=========

A1 Policy is sent to Traffic Steering xApp to define the Intent which will drive the Traffic Steering behavior.

Policy Type ID is 20008.

Currently, there is only one parameter that can be provided in A1 Policy: *threshold*

An example Policy follows:

.. code-block::

    { "threshold": 5 }

This Policy instructs Traffic Steering xApp to hand-off any UE whose downlink throughput of its current serving cell is 5% below the throughput of any neighboring cell.

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

Sending QoE Prediction Request
==============================

Traffic Steering listens for badly performing UEs.
Each Anomaly Detection message received from AD xApp, results in a QoE Prediction Request to QP xApp.
The RMR Message Type is 30000.
The following is an example message body:

.. code-block::

    { "UEPredictionSet": ["Train passenger 2"] }

Receiving QoE Prediction
========================

Traffic Steering xApp defines a callback for QoE Prediction received from QP xApp. The RMR message type is 30002. The following is an example message body:

.. code-block::

    {
        "Train passenger 2":{
            "310-680-200-555001":[2000000, 1200000],
            "310-680-200-555002":[1000000, 4000000],
            "310-680-200-555003":[5000000, 4000000]
        }
    }

This message provides throughput predictions of three cells for the UE ID "Train passenger 2". For each service cell, it lists an array containing two elements: DL Throughput and UL Throughput predictions.

Traffic Steering xApp checks for the Service Cell ID for UE ID, and determines if the predicted throughput is higher in a neighbor cell.
The first cell in this prediction message is assumed to be the serving cell.

If predicted throughput is higher than the A1 policy "*threshold*" in a given neighbor cell, Traffic Steering sends the CONTROL message to a given endpoint.
Since RC xApp is not mandatory for the Traffic Steering use case, TS xApp sends CONTROL messages using either REST or gRPC calls.
The CONTROL endpoint is set up in the xApp descriptor file called "config-file.json". Please, check out the "schema.json" file for configuration examples.

The following is an example of a REST message that requests the handover of a given UE:

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

Control messages might also be exchanged with E2 Simulators that implement REST-based interfaces.
Traffic Steering then logs the REST response showing whether or not the control operation has succeeded.

The gRPC interface is only required to exchange messages with the RC xApp.
The following is an example of the gRPC message (*string representation*) which requests the RC xApp to handover a given UE:

.. code-block::

    e2NodeID: "000000000001001000110100"
    plmnID: "02F829"
    ranName: "enb_208_092_001235"
        RICE2APHeaderData {
        RanFuncId: 300
        RICRequestorID: 1001
    }
    RICControlHeaderData {
        ControlStyle: 3
        ControlActionId: 1
        UEID: "Train passenger 2"
    }
    RICControlMessageData {
        TargetCellID: "mnop"
    }

TS xApp also requires to fetch additional RAN information from the E2 Manager to communicate with RC xApp.
By default, TS xApp requests information to the default endpoint of E2 Manager in the Kubernetes cluster. Currently, this is done once on startup.
Finally, the default E2 Manager endpoint from TS can be changed using the env variable "SERVICE_E2MGR_HTTP_BASE_URL".
