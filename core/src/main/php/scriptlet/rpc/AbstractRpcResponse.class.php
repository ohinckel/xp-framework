<?php
/* This class is part of the XP framework
 *
 * $Id$ 
 */

  uses('scriptlet.HttpScriptletResponse', 'util.log.Traceable');

  /**
   * RPC response object
   *
   * @see      xp://scriptlet.rpc.AbstractRpcRouter
   * @purpose  Response
   */
  class AbstractRpcResponse extends HttpScriptletResponse implements Traceable {
    public
      $message  = NULL;
    
    public
      $cat      = NULL;
    
    /**
     * Constructor
     *
     */
    public function __construct() {
      $this->setHeader('Server', 'Abstract RPC 1.0 / PHP'.phpversion().' / XP Framework');
    }
    
    /**
     * Sets message object
     *
     * @param   scriptlet.rpc.AbstractRpcMessage msg
     */
    public function setMessage($msg) {
      $this->message= $msg;
    }
    
    /**
     * Set trace for debugging
     *
     * @param   util.log.LogCategory cat
     */
    public function setTrace($cat) {
      $this->cat= $cat;
    }
  } 
?>
