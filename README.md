# ACN_Traceroute
Advanced Computer Networks-Traceroute ICMP Packet Processing

Purpose:
To implement a basic prototype of the Expanding Ring Search (ERS) routing mechanism with functionality similar to the traceroute tool.

Program Features:

Environment Requirements:

Operates under Ubuntu 24.04 OS.
Written in the C language with a Makefile for compilation.
Core Functionalities:

Route Discovery via Hop Distance:
Allows users to identify the router at a specific hop distance to a destination.
Example usage: prog <hop-distance> <destination>.
E.g., prog 3 140.117.11.1 outputs the router 3 hops away from the source on the route to the given destination.
Flooding with TTL Control:
Implements a Time-to-Live (TTL) mechanism to control flooding and minimize energy waste.
Incremental TTL values are used to expand the search zone gradually, one "ring" at a time, starting from the source node.
Controlled Flooding:
Utilizes RREQ (Route Request) and RREP (Route Reply) signals for effective control of flooding during route discovery.
Energy Efficiency:
Avoids redundant rebroadcasts by incrementally expanding the search area, reducing overlap with previously covered zones.
ERS Concept:

Implements a search mechanism inspired by ERS:
Starts with a small search area and incrementally expands it if no route is found.
Continues until the router is discovered or the maximum search area is reached.
Motivation and Background:

The implementation simulates routing protocols using controlled flooding and TTL mechanisms.
Highlights inefficiencies in traditional ERS due to overlapping search areas and energy waste.






