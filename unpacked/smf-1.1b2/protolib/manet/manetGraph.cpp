#include "manetGraph.h"
#include <protoDebug.h>


ManetGraph::Cost::Cost()
 : value(0.0)
{
}

ManetGraph::Cost::~Cost()
{
}

const double ManetGraph::Cost::MAX_VALUE = -1.0;

void ManetGraph::Cost::operator+=(const Cost& cost)
{
    if (IsMax())
        return;
    else if (cost.IsMax())
        Maximize();
    else
        value += cost.value;
}  // end ManetGraph::Cost::operator+=()

ManetGraph::Link::Link(Interface& a, Interface& b)
 : Edge(a, b)
{  
}

ManetGraph::Link::~Link()
{   
}

void ManetGraph::Link::SetCost(const Interface& src, const Cost& theCost)
{
    ASSERT((&src == &term_a) || (&src == &term_b));
    if (&src == &term_a)
        cost_a = theCost;
    else
        cost_b = theCost;   
}  // end ManetGraph::Link::SetCost()

const ManetGraph::Cost& ManetGraph::Link::GetCost(const Interface& src) const
{
    ASSERT((&src == &term_a) || (&src == &term_b));
    return ((&src == &term_a) ? cost_a : cost_b);
}  // end ManetGraph::Link::GetCost()

void ManetGraph::Link::Select(const Interface& src)
{
    ASSERT((&src == &term_a) || (&src == &term_b));
    if (&src == &term_a)
        select_a = true;
    else
        select_b = true;   
}  // end ManetGraph::Link::Select()

void ManetGraph::Link::Unselect(const Interface& src)
{
    ASSERT((&src == &term_a) || (&src == &term_b));
    if (&src == &term_a)
        select_a = false;
    else
        select_b = false;   
}  // end ManetGraph::Link::Unselect()

bool ManetGraph::Link::WasSelected(const Interface& src) const
{
    ASSERT((&src == &term_a) || (&src == &term_b));
    return ((&src == &term_a) ? select_a : select_b);
}  // end ManetGraph::Link::WasSelected()
        
ManetGraph::Interface::Interface(Node& theNode, const ProtoAddress& addr)
 : node(theNode), address(addr),  
   parent_link(NULL), next_hop_link(NULL),
   next(NULL)
{
}

ManetGraph::Interface::~Interface()
{   
} 

bool ManetGraph::Interface::Connect(Interface& iface, const Cost& linkCost, bool duplex)
{ 
    // 1) Does the link already exist?
    LinkIterator linkIterator(*this);
    Link* link;
    while (NULL != (link = linkIterator.GetNextLink()))
    {
        if (&iface == &link->GetDst(*this))
            break;
    }
    // 2) If not, create it
    if (NULL == link)
    {
        if (NULL == (link = new Link(*this, iface)))
        {
            DMSG(0, "ManetGraph::Interface::Connect() new Link error: %s\n",
                    GetErrorString());
            return false;
        }
    }
    // 3) Set the link cost from "this" -> "iface"
    link->SetCost(*this, linkCost);
    
    
    // 4) Make it bi-directional if "duplex"
    if (duplex)
    {
        if (!link->GetDst(*this).Connect(*this, linkCost, false))
        {
            DMSG(0, "ManetGraph::Interface::Connect() error setting duplex link\n");
        }
    }
    return true;
}  // end ManetGraph::Interface::Link()

ManetGraph::Interface::LinkIterator::LinkIterator(Interface& theInterface)
 : EdgeIterator(static_cast<Vertice&>(theInterface))
{
}

ManetGraph::Interface::LinkIterator::~LinkIterator()
{
}

ManetGraph::Interface::PriorityQueue::PriorityQueue()
{
}

ManetGraph::Interface::PriorityQueue::~PriorityQueue()
{
}


// Insert iface from top (head) of queue
void ManetGraph::Interface::PriorityQueue::InsertFromHead(Interface& iface)
{
    const Cost& theCost = iface.GetCost();
    PriorityQueue::Iterator iterator(*this, false);  // forward iterator
    Interface* nextIface;
    while (NULL != (nextIface = iterator.GetNextInterface()))
    {
        if (theCost <= nextIface->GetCost())
        {
            InsertBefore(iface, *nextIface);
            return;
        }
    }   
    Append(iface);
}  // end ManetGraph::Interface::PriorityQueue::InsertFromHead()

// Insert iface from bottom (tail) of queue
void ManetGraph::Interface::PriorityQueue::InsertFromTail(Interface& iface)
{
    const Cost& theCost = iface.GetCost();
    PriorityQueue::Iterator iterator(*this, true);  // reverse iterator
    Interface* prevIface;
    while (NULL != (prevIface = iterator.GetNextInterface()))
    {
        if (theCost >= prevIface->GetCost())
        {
            InsertAfter(iface, *prevIface);
            return;
        }
    }   
    Prepend(iface);
}  // end ManetGraph::Interface::PriorityQueue::InsertFromTail()


void ManetGraph::Interface::PriorityQueue::Adjust(Interface& iface, const Cost& newCost)
{
    const Cost& oldCost = iface.GetCost();
    if (newCost == oldCost) 
    {
        return;  // no adjustment needed.
    }
    else if (newCost.IsMax())
    {
        Remove(iface);
        iface.SetCost(newCost);
        Append(iface);
    }
    else if (newCost < oldCost)
    {
        // This removes the vertice and reinserts starting from "head"
        // (TBD) Improve our priority queue implementation so such
        //       shuffling is as efficient as possible
        Remove(iface);
        iface.SetCost(newCost);
        PriorityQueue::Iterator iterator(*this);
        Interface* nextIface;
        while (NULL != (nextIface = iterator.GetNextInterface()))
        {
            if (newCost <= nextIface->GetCost())
            {
                InsertBefore(iface, *nextIface);
                return;
            }
        }   
        Append(iface);
    }
    else  // newCost > oldCost
    {
        // Slide down from current position, or insert from tail?
        ASSERT(0); // (TBD) allow for this although won't happen w/ Dijkstra
    }
    
}  // end ProtoGraph::Vertice::PriorityQueue::Adjust()


ManetGraph::Interface::PriorityQueue::Iterator::Iterator(PriorityQueue& theQueue, bool reverse)
 : Queue::Iterator(static_cast<Queue&>(theQueue), reverse)
{
}

ManetGraph::Interface::PriorityQueue::Iterator::~Iterator()
{
}


ManetGraph::Node::Node(const ProtoAddress& defaultAddr)
 : default_interface(*this, defaultAddr)
{
}

ManetGraph::Node::~Node()
{
    // Delete any extra interfaces
    Interface* next = default_interface.GetNext();
    while (NULL != next)
    {
        Interface* iface = next;
        Interface* next = next->GetNext();
        delete iface;
    }
}       

void ManetGraph::Node::AppendInterface(Interface& iface)
{
    iface.Append(NULL);
    Interface* prev = NULL;
    Interface* next = &default_interface;
    while (next)
    {
        prev = next;
        next = next->GetNext();   
    }
    prev->Append(&iface);
}  // end ManetGraph::Node::AppendInterface()

void ManetGraph::Node::RemoveInterface(Interface& iface)
{
    ASSERT(&iface != &default_interface);
    Interface* prev = NULL;
    Interface* next = &default_interface;
    while (next)
    {
        if (&iface == &default_interface)
        {
            DMSG(0, "ManetGraph::Node::RemoveInterface() error: can't remove default_interface\n");
        }
        else if (&iface == next)
        {
            prev->Append(iface.GetNext());
            return;
        }
        prev = next;
        next = next->GetNext();   
    }
}  // end ManetGraph::Node::RemoveInterface()

ManetGraph::ManetGraph()
{
}

ManetGraph::~ManetGraph()
{
}

bool ManetGraph::InsertNode(Node& node, Interface* iface)
{
    if (NULL == iface) iface = &node.GetDefaultInterface();
    return ProtoGraph::InsertVertice(*iface);
}  // end ManetGraph::InsertNode()

void ManetGraph::RemoveNode(Node& node, Interface* iface)
{
    if (NULL == iface)
    {
        // Remove all interfaces associated with this node
        iface = &node.GetDefaultInterface();
        while (NULL != iface)
        {
            RemoveVertice(static_cast<Vertice&>(*iface));
            iface = iface->GetNext();
        }
    }
    else
    {   
        // Remove only the interface specified
        RemoveVertice(static_cast<Vertice&>(*iface));
    }                                  
}  // end ManetGraph::RemoveNode()


ManetGraph::InterfaceIterator::InterfaceIterator(ManetGraph& theGraph)
  : ProtoGraph::VerticeIterator(static_cast<ProtoGraph&>(theGraph))
{
}

ManetGraph::InterfaceIterator::~InterfaceIterator()
{
}

ManetGraph::DijkstraTraversal::DijkstraTraversal(ManetGraph& theGraph, 
                                                 Node&       startNode,
                                                 Interface*  startIface)
 : manet_graph(theGraph),
   start_iface((NULL != startIface) ? *startIface : startNode.GetDefaultInterface()),
   trans_iface(NULL), current_level(0)
{
    ASSERT(&start_iface.GetNode() == &startNode);
    Reset();
}

ManetGraph::DijkstraTraversal::~DijkstraTraversal()
{
}

void ManetGraph::DijkstraTraversal::Reset()
{
    // (TBD) use a dual queue approach so that we can avoid 
    //      visiting every iface twice everytime we run the Dijkstra.
    //      I.e., we could maintain "visited" & "unvisited" queue, removing ifaces
    //      from that queue as they are visited, and then at the end of
    //      the Dijkstra, update the cost of any remaining unvisited ifaces
    //      to COST_MAX and then swap visited and unvisited queues for next
    //      Dijkstra run ... Thus each iface would be visited only once _or_ as
    //      needed for Dijkstra instead of once _plus_ as needed for Dijkstra
    queue.Empty();
    InterfaceIterator ifaceIterator(manet_graph);    
    Interface* nextIface;
    while (NULL != (nextIface = ifaceIterator.GetNextInterface()))
    {
        if (nextIface == &start_iface)
        {
            nextIface->MinimizeCost();  // initialize
            queue.Prepend(*nextIface);
        }
        else
        {
            nextIface->MaximizeCost();  // initialize
        }
    }
    dijkstra_completed = false;
}  // end ManetGraph::DijkstraTraversal::Reset()

// Dijkstra traversal step
ManetGraph::Interface* ManetGraph::DijkstraTraversal::GetNextInterface()
{
    Interface* currentIface = static_cast<Interface*>(queue.RemoveHead());
    if (NULL != currentIface)
    {
        const Cost& currentCost = currentIface->GetCost();
        ASSERT(!currentCost.IsMax());
        Interface::LinkIterator linkIterator(*currentIface);
        Link* nextLink;
        while ((nextLink = linkIterator.GetNextLink()))
        {
            nextLink->Unselect(*currentIface);
            // (TBD) we could do something different here,
            //       like have a virtual comparator method
            //       so this Dijkstra would work on derived
            //       variations of the "Cost", "Link", and 
            //       "Interface" classes
            Cost nextCost = nextLink->GetCost(*currentIface);
            nextCost += currentCost;
            Interface& nextDst = nextLink->GetDst(*currentIface);
            if (nextCost < nextDst.GetCost()) 
            {
                if (nextDst.GetCost().IsMax())
                {
                   // This is the first path found
                    nextDst.SetCost(nextCost);
                    queue.InsertFromTail(nextDst); 
                }
                else
                {
                    // We found a shorter path 
                    // (never called when links are of homogeneous cost)
                    queue.Adjust(nextDst, nextCost);
                }

                // Update shortest path info
                nextDst.SetParentLink(nextLink);
                if (currentIface == &start_iface)
                    nextDst.SetNextHopLink(nextLink);
                else
                    nextDst.SetNextHopLink(currentIface->GetNextHopLink());
            }
        }
        Link* parentLink = currentIface->GetParentLink();
        if (NULL != parentLink)
            parentLink->Select(parentLink->GetDst(*currentIface));
    }
    else
    {
        dijkstra_completed = true;
    }
    return currentIface;
}  // end ManetGraph::DijkstraTraversal::GetNextInterface()

void ManetGraph::DijkstraTraversal::Update(Interface& startIface)
{
    if (!dijkstra_completed)
    {
        // Complete dijkstra traversal
        while (NULL != GetNextInterface());
    }
    queue.Append(startIface);
    while (NULL != GetNextInterface());
}  // end ManetGraph::DijkstraTraversal::Update()


// Call this to setup re-traverse of tree computed via Dijkstra
void ManetGraph::DijkstraTraversal::TreeWalkReset()
{
    // If Dijkstra has not completed, run full Dijkstra
    if (!dijkstra_completed)
    {
        Reset();
        while (NULL != GetNextInterface());
    }
    queue.Empty();
    queue.Append(start_iface);
    trans_iface = NULL;
    current_level = 0;
}  // end MrbNodeTree::Traversal::TreeWalkReset()

ManetGraph::Interface* ManetGraph::DijkstraTraversal::TreeWalkNext(unsigned int* level)
{
    Interface* currentIface = static_cast<Interface*>(queue.RemoveHead());
    if (NULL != currentIface)
    {
        // Find selected links
        Interface::LinkIterator linkIterator(*currentIface);
        Link* nextLink;
        Link* firstLink = NULL;
        while ((nextLink = linkIterator.GetNextLink()))
        {
            if (nextLink->WasSelected(*currentIface))
            {
                if (NULL == firstLink) firstLink = nextLink;
                queue.Append(nextLink->GetDst(*currentIface));
            }
        }
        // Track depth as walk progresses ...
        if (NULL == trans_iface)
        {
            trans_iface = firstLink ? &firstLink->GetDst(*currentIface) : NULL;
        }
        else if (trans_iface == currentIface)
        {
            trans_iface = firstLink ? &firstLink->GetDst(*currentIface) : NULL; 
            current_level++;  
        }
        if (NULL != level) *level = current_level;
    }
    return currentIface;
}  // end ManetGraph::DijkstraTraversal::TreeWalkNext()

