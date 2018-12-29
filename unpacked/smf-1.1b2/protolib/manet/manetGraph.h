#ifndef _MANET_GRAPH
#define _MANET_GRAPH

#include "protoGraph.h"
#include <protoAddress.h>

class ManetGraph : public ProtoGraph
{
    public:
        ManetGraph();
        ~ManetGraph();
        
        class Cost
        {
            public:
                Cost();
                virtual ~Cost();
                
                // Value manipulation methods
                void SetValue(double theValue) 
                    {value = (theValue >= 0.0) ? theValue : MAX_VALUE;}
                void Minimize() 
                    {value = 0.0;}
                void Maximize() 
                    {value = MAX_VALUE;}
                void operator+=(const Cost& cost);
                
                // Value query const methods
                double GetValue() const {return value;}
                bool IsMax() const
                    {return MAX_VALUE == value;}
                bool operator==(const Cost& cost) const
                    {return (cost.value == value);}
                bool operator!=(const Cost& cost) const
                    {return (cost.value != value);}
                bool operator<(const Cost& cost) const
                {
                    return (IsMax() ? 
                                false : 
                                (cost.IsMax() ? 
                                    true : 
                                    value < cost.value));
                }
                bool operator<=(const Cost& cost) const
                    {return ((*this == cost) || (*this < cost));}
                bool operator>(const Cost& cost) const
                {
                    return (cost.IsMax() ? 
                                false :  
                                IsMax() ?
                                    true :
                                    value > cost.value);  
                }
                bool operator>=(const Cost& cost) const
                    {return ((*this == cost) || (*this > cost));}

            private:
                static const double MAX_VALUE;
            
                double value;

        };  // end class ManetGraph::Cost

        class Interface;

        class Link : public Edge
        {
            public:
                Link(Interface& a, Interface& b);
                ~Link();
                
                Interface& GetDst(const Interface& src) const
                    {return static_cast<Interface&>(Edge::GetDst(src));}

                const Cost& GetCost(const Interface& src) const;
                void SetCost(const Interface& src, const Cost& cost);

                void Select(const Interface& src);
                void Unselect(const Interface& src);
                bool WasSelected(const Interface& src) const;

            private:
                Cost    cost_a;  // cost to 'term_a' from 'term_b'
                bool    select_a;
                Cost    cost_b;  // cost to 'term_b' from 'term_a'
                bool    select_b;

        };  // end class ManetGraph::Link

        class Node;
        class Interface : public ProtoGraph::Vertice
        {
            
            public:
                Interface(Node& theNode, const ProtoAddress& addr);
                ~Interface();    
                
                // Implementation of required overrides
                virtual const char* GetKey() const {return address.GetRawHostAddress();}
                virtual unsigned int GetKeyLength() const {return (address.GetLength() * 8);}

                const ProtoAddress& GetAddress() const 
                    {return address;}
                void SetAddress(const ProtoAddress& theAddress)
                    {address = theAddress;}

                Node& GetNode() const {return node;}
                
                void Append(Interface* iface) {next = iface;}
                Interface* GetNext() const {return next;}

                void SetCost(const Cost& theCost) {cost = theCost;}
                void MaximizeCost() {cost.Maximize();}
                void MinimizeCost() {cost.Minimize();}
                const Cost& GetCost() const {return cost;}

                // Add/Mod link from this -> iface
                bool Connect(Interface& iface, const Cost& theCost, bool duplex = true);

                // These two are useful post-Djikstra
                Interface* GetNextHop(const Interface& src) const
                    {return (next_hop_link ? 
                                static_cast<Interface*>(&next_hop_link->GetDst(src)) : NULL);}
                Interface* GetPrevHop() const
                    {return (parent_link ? 
                                static_cast<Interface*>(&parent_link->GetDst(*this)) : NULL);}

                void SetParentLink(Link* parentLink)
                    {parent_link = parentLink;}
                Link* GetParentLink() const {return parent_link;}

                void SetNextHopLink(Link* nextHopLink) {next_hop_link = nextHopLink;}
                Link* GetNextHopLink() {return next_hop_link;}
                
                void SetLevel(unsigned int theLevel) {level = theLevel;}
                unsigned int GetLevel() {return level;}


                class LinkIterator : public Vertice::EdgeIterator
                {
                    public:
                        LinkIterator(Interface& theInterface);
                        ~LinkIterator();

                        void Reset() 
                            {EdgeIterator::Reset();}

                        Link* GetNextLink()
                            {return static_cast<Link*>(GetNextEdge());}
                };  // end class Interface::LinkIterator
                
                
                // Queue class to use for _temporary_ traversal purposes
                // (uses Interface "queue_prev" & "queue_next" members
                class PriorityQueue : public Vertice::Queue
                {
                    public:
                        PriorityQueue();
                        ~PriorityQueue();
                        
                        void Adjust(Interface& iface, const Cost& newCost);
                        void InsertFromHead(Interface& iface);
                        void InsertFromTail(Interface& iface);

                        class Iterator : public Queue::Iterator
                        {
                            public:
                                Iterator(PriorityQueue& theQueue, bool reverse = false);
                                ~Iterator();

                                void Reset() 
                                    {Queue::Iterator::Reset();}
                                Interface* GetNextInterface()
                                    {return static_cast<Interface*>(GetNextVertice());}

                        };  // end class MrbInterface::Queue::Iterator
                };  // end class ManetGraph::Interface::PriorityQueue


            private:

                Node&           node;
                ProtoAddress    address;

                // Dijkstra state variables
                Cost            cost;             // cumulative cost from tree root to here
                Link*           parent_link;      // reverse path towards tree root
                Link*           next_hop_link;    // forward path from tree root   

                // BFS state variables
                unsigned int    level;            // Dijkstra tree level

                Interface*      next;             // for Node's interface list

        };  // end class ManetGraph::Interface
        
        
        class Node
        {
            public:
                Node(const ProtoAddress& defaultAddr);
                ~Node();

                const ProtoAddress& GetDefaultAddress() const
                    {return default_interface.GetAddress();}

                Interface& GetDefaultInterface() {return default_interface;} 
                void AppendInterface(Interface& iface);
                void RemoveInterface(Interface& iface);

            private:
                ManetGraph::Interface   default_interface;

        };  // end class ManetGraph::Node
                
        // ManetGraph control/query methods
        bool InsertNode(Node& node, Interface* iface = NULL);
        void RemoveNode(Node& node, Interface* iface = NULL);
        Interface* FindInterface(ProtoAddress& theAddress)
        {
            const char* key = theAddress.GetRawHostAddress();
            unsigned int keyBits = theAddress.GetLength() * 8;
            return static_cast<Interface*>(FindVertice(key, keyBits));
        }
        
        Node* FindNode(ProtoAddress& theAddress)
        {
            Interface* iface = FindInterface(theAddress);
            return (NULL != iface) ? &iface->GetNode() : NULL;
        }
        
        class InterfaceIterator  : public ProtoGraph::VerticeIterator
        {
            public:
                InterfaceIterator(ManetGraph& theGraph);
                ~InterfaceIterator();

                void Reset() {ProtoGraph::VerticeIterator::Reset();}

                Interface* GetNextInterface()
                    {return static_cast<Interface*>(GetNextVertice());}
                
        };  // end class ManetGraph::InterfaceIterator
        
        class DijkstraTraversal
        {
            public:
                DijkstraTraversal(ManetGraph&  theGraph, 
                                  Node&        startNode,
                                  Interface*   startIface = NULL);
                ~DijkstraTraversal();
                
                void Reset();
                Interface* GetNextInterface();
                
                void Update(Interface& startIface);
                
                // BFS traversal of routing tree ("tree walk")
                // (use post-Dijkstra (after "GetNextInterface" returns NULL)
                void TreeWalkReset();
                Interface* TreeWalkNext(unsigned int* level = NULL);
                
            private:
                ManetGraph&                 manet_graph;
                Interface&                  start_iface;
                Interface::PriorityQueue    queue;
                
                Interface*                  trans_iface;
                unsigned int                current_level;
                
                bool                        dijkstra_completed;
               
        };  // end class ManetGraph::DijkstraTraversal         
};  // end class ManetGraph

#endif // _MANET_GRAPH
