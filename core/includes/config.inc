<?php

/**
 * @file
 * This is the API for configuration storage.
 */

/**
 * Gets the randomly generated config directory name.
 *
 * @return
 *   The directory name.
 */
function config_get_config_directory() {
  global $config_directory_name;

  return conf_path() . '/files/' . $config_directory_name;
}

/**
 * Moves the default config supplied by a module to the live config directory.
 *
 * @param
 *   The name of the module we are installing.
 */
function config_install_default_config($module) {
  $module_config_dir = drupal_get_path('module', $module) . '/config';
  $drupal_config_dir = config_get_config_directory();
  if (is_dir(drupal_get_path('module', $module) . '/config')) {
    $files = glob($module_config_dir . '/' . '*.xml');
    foreach ($files as $key => $file) {
      // Load config data into the active store and write it out to the
      // file system in the drupal config directory. Note the config name
      // needs to be the same as the file name WITHOUT the extension.
      // @todo Make this acknowledge other storage engines rather than having
      //   SQL be hardcoded.
      $parts = explode('/', $file);
      $file = array_pop($parts);
      $config_name = str_replace('.xml', '', $file);

      $verified_storage = new DrupalVerifiedStorageSQL($config_name);
      $verified_storage->write(file_get_contents($module_config_dir . '/' . $file));
    }
  }
}

/**
 * Retrieves an iterable array which lists the children under a config 'branch'.
 *
 * Given the following configuration files:
 * - core.entity.node_type.article.xml
 * - core.entity.node_type.page.xml
 *
 * You can pass a prefix 'core.entity.node_type' and get back an array of the
 * filenames that match. This allows you to iterate through all files in a
 * branch. Note that this will only work on the level above the tips, so
 * a prefix of 'core.entity' would return an empty array.
 *
 * @param $prefix
 *   The prefix of the files we are searching for.
 *
 * @return
 *   An array of file names under a branch.
 */
function config_get_signed_file_storage_names_with_prefix($prefix = '') {
  $files = glob(config_get_config_directory() . '/' . $prefix . '*.xml');
  $clean_name = function ($value) {
    return basename($value, '.xml');
  };
  return array_map($clean_name, $files);
}

/**
 * Generates a hash of a config file's contents using our encryption key.
 *
 * @param $data
 *   The contents of a configuration file.
 *
 * @return
 *   A hash of the data.
 */
function config_sign_data($data) {
  // The configuration key is loaded from settings.php and imported into the global namespace
  global $config_signature_key;

  // SHA-512 is both secure and very fast on 64 bit CPUs.
  // @todo What about 32-bit CPUs?
  return hash_hmac('sha512', $data, $config_signature_key);
}

/**
 * @todo
 *
 * @param $prefix
 *   @todo
 *
 * @return
 *   @todo
 */
function config_get_verified_storage_names_with_prefix($prefix = '') {
  return DrupalVerifiedStorageSQL::getNamesWithPrefix($prefix);
}

/**
 * @todo
 *
 * @param $prefix
 *   @todo
 *
 * @return
 *   @todo
 */
function config_get_names_with_prefix($prefix) {
  return config_get_verified_storage_names_with_prefix($prefix);
}

/**
 * Retrieves a configuration object.
 *
 * This is the main entry point to the configuration API. Calling
 * @code config(book.admin) @endcode will return a configuration object in which
 * the book module can store its administrative settings.
 *
 * @param $name
 *   The name of the configuration object to retrieve. The name corresponds to
 *   an XML configuration file. For @code config(book.admin) @endcode, the
 *   config object returned will contain the contents of book.admin.xml.
 * @param $class
 *   The class name of the config object to be returned. Defaults to
 *   DrupalConfig.
 *
 * @return
 *   An instance of the class specified in the $class parameter.
 *
 */
function config($name, $class = 'DrupalConfig') {
  // @todo Replace this with an appropriate factory.
  return new $class(new DrupalVerifiedStorageSQL($name));
}

/**
 * Decodes configuration data from its native format to an associative array.
 *
 * @param $data
 *   Configuration data.
 *
 * @return
 *   An associative array representation of the data.
 */
function config_decode($data) {
  if (empty($data)) {
    return array();
  }
  $xml = new SimpleXMLElement($data);
  $json = json_encode($xml);
  return json_decode($json, TRUE);
}

/**
 * Standardizes SimpleXML object output into simple arrays for easier use.
 *
 * @param $xmlObject
 *   A valid XML string.
 *
 * @return
 *   An array representation of A SimpleXML object.
 */
function config_xml_to_array($data) {
  $out = array();
  $xmlObject = simplexml_load_string($data);

  if (is_object($xmlObject)) {
    $attributes = (array) $xmlObject->attributes();
    if (isset($attributes['@attributes'])) {
      $out['#attributes'] = $attributes['@attributes'];
    }
  }
  if (trim((string) $xmlObject)) {
    return trim((string) $xmlObject);
  }
  foreach ($xmlObject as $index => $content) {
    if (is_object($content)) {
      $out[$index] = config_xml2array($content);
    }
  }

  return $out;
}

/**
 * Encodes an array into the native configuration format.
 *
 * @param $data
 *   An associative array or an object
 *
 * @return
 *   A representation of this array or object in the native configuration
 *   format.
 *
 * @todo This needs to work for objects as well and currently doesn't.
 */
function config_encode($data) {
  // creating object of SimpleXMLElement
  $xml_object = new SimpleXMLElement("<?xml version=\"1.0\"?><config></config>");

  // function call to convert array to xml
  config_array_to_xml($data, $xml_object);

  // Pretty print the result
  $dom = new DOMDocument('1.0');
  $dom->preserveWhiteSpace = false;
  $dom->formatOutput = true;
  $dom->loadXML($xml_object->asXML());
  return $dom->saveXML();
}

/**
 * Encodes an array into XML
 *
 * @param $data
 *   An associative array or an object
 *
 * @return
 *   A representation of this array or object in the native configuration
 *   format.
 *
 * @todo This needs to work for objects as well and currently doesn't.
 */
function config_array_to_xml($array, &$xml_object) {
  foreach ($array as $key => $value) {
    if (is_array($value)) {
      if (!is_numeric($key)){
        $subnode = $xml_object->addChild("$key");
        config_array_to_xml($value, $subnode);
      }
      else {
        config_array_to_xml($value, $xml_object);
      }
    }
    else {
      $xml_object->addChild("$key", "$value");
    }
  }
}

/**
 * @todo
 */
class ConfigException extends Exception {}

/**
 * @todo
 */
class ConfigFileStorageException extends ConfigException {}

/**
 * @todo
 */
class ConfigFileStorageReadException extends ConfigFileStorageException {}

/**
 * @todo
 */
class ConfigFileStorageSignatureException extends ConfigFileStorageException {}

/**
 * Represents the signed file storage interface.
 *
 * Classes implementing this interface allow reading and writing configuration
 * data to and from disk, while automatically managing and verifying
 * cryptographic signatures.
 */
class SignedFileStorage {

  /**
   * Constructs a SignedFileStorage object.
   *
   * @param string $name
   *   The name for the configuration data. Should be lowercase.
   */
  public function __construct($name) {
    $this->name = $name;
  }

  /**
   * Reads and returns a signed file and its signature.
   *
   * @return
   *   An array with "signature" and "data" keys.
   *
   * @throws
   *   Exception
   */
  protected function readWithSignature() {
    // @todo Optimize with explicit offsets?
    $content = file_get_contents($this->getFilePath());
    if ($content === FALSE) {
      throw new Exception('Read file is invalid.');
    }
    $signature = file_get_contents($this->getFilePath() . '.sig');
    if ($signature === FALSE) {
      throw new Exception('Signature file is invalid.');
    }
    return array('data' => $content, 'signature' => $signature);
  }

  /**
   * Checks whether the XML configuration file already exists on disk.
   *
   * @return
   *   @todo
   */
  protected function exists() {
    return file_exists($this->getFilePath());
  }

  /**
   * Returns the path to the XML configuration file.
   *
   * @return
   *   @todo
   */
  public function getFilePath() {
    return config_get_config_directory() . '/' . $this->name  . '.xml';
  }

  /**
   * Recreates the signature for the file.
   */
  public function resign() {
    if ($this->exists()) {
      $parts = $this->readWithSignature();
      $this->write($parts['data']);
    }
  }

  /**
   * Cryptographically verifies the integrity of the configuration file.
   *
   * @param $contentOnSuccess
   *   Whether or not to return the contents of the verified configuration file.
   *
   * @return mixed
   *   If $contentOnSuccess was TRUE, returns the contents of the verified
   *   configuration file; otherwise returns TRUE on success. Always returns
   *   FALSE if the configuration file was not successfully verified.
   */
  public function verify($contentOnSuccess = FALSE) {
    if ($this->exists()) {
      $split = $this->readWithSignature();
      $expected_signature = config_sign_data($split['data']);
      if ($expected_signature === $split['signature']) {
        if ($contentOnSuccess) {
          return $split['data'];
        }
        return TRUE;
      }
    }
    return FALSE;
  }

  /**
   * Writes the contents of the configuration file to disk.
   *
   * @param $data
   *   The data to be written to the file.
   *
   * @throws
   *   Exception
   *
   * @todo What format is $data in?
   */
  public function write($data) {
    $signature = config_sign_data($data);
    if (!file_put_contents($this->getFilePath(), $data)) {
      throw new Exception('Failed to write configuration file.');
    }
    if (!file_put_contents($this->getFilePath() . '.sig', $signature)) {
      throw new Exception('Failed to write signature file.');
    }
  }

  /**
   * Returns the contents of the configuration file.
   *
   * @return
   *   @todo
   */
  public function read() {
    if ($this->exists()) {
      $verification = $this->verify(TRUE);
      if ($verification === FALSE) {
        throw new Exception('Invalid signature in file header.');
      }
      return $verification;
    }
  }

  /**
   * Deletes a configuration file.
   */
  public function delete() {
    // Needs error handling and etc.
    @drupal_unlink($this->getFilePath());
    @drupal_unlink($this->getFilePath() . '.sig');
  }
}

/**
 * Defines an interface for verified storage manipulation.
 *
 * This class allows reading and writing configuration data from/to the
 * verified storage and copying to/from the signed file storing the same data.
 */
interface DrupalConfigVerifiedStorageInterface {

  /**
   * Constructs a verified storage manipulation class.
   *
   * @param $name
   *   Lowercase string, the name for the configuration data.
   */
  function __construct($name);

  /**
   * Reads the configuration data from the verified storage.
   */
  function read();

  /**
   * Copies the configuration data from the verified storage into a file.
   */
  function copyToFile();

  /**
   * Copies the configuration data from the file into the verified storage.
   */
  function copyFromFile();

  /**
   * Deletes the configuration data file.
   */
  function deleteFile();

  /**
   * Checks whether the file and the verified storage is in sync.
   *
   * @return
   *   TRUE if the file and the verified storage contains the same data, FALSE
   *   if not.
   */
  function isOutOfSync();

  /**
   * Writes the configuration data into the active storage but not the file.
   *
   * Use this function if you need to make temporary changes to your
   * configuration.
   *
   * @param $data
   *   The configuration data to write into active storage.
   */
  function writeToActive($data);

  /**
   * Writes the configuration data into the active storage and the file.
   *
   * @param $data
   *   The configuration data to write.
   */
  function write($data);

  /**
   * Gets names starting with this prefix.
   *
   * @param $prefix
   *   @todo
   */
  static function getNamesWithPrefix($prefix);
}

/**
 * @todo
 */
abstract class DrupalConfigVerifiedStorage implements DrupalConfigVerifiedStorageInterface {

  /**
   * Implements DrupalConfigVerifiedStorageInterface::__construct().
   */
  function __construct($name) {
    $this->name = $name;
  }

  /**
   * @todo
   *
   * @return
   *   @todo
   */
  protected function signedFileStorage() {
    return new SignedFileStorage($this->name);
  }

  /**
   * Implements DrupalConfigVerifiedStorageInterface::copyToFile().
   */
  public function copyToFile() {
    return $this->signedFileStorage()->write($this->read());
  }

  /**
   * Implements DrupalConfigVerifiedStorageInterface::deleteFile().
   */
  public function deleteFile() {
    return $this->signedFileStorage()->delete();
  }

  /**
   * Implements DrupalConfigVerifiedStorageInterface::copyFromFile().
   */
  public function copyFromFile() {
    return $this->writeToActive($this->readFromFile());
  }

  /**
   * @todo
   *
   * @return
   *   @todo
   */
  public function readFromFile() {
    return $this->signedFileStorage()->read($this->name);
  }

  /**
   * Implements DrupalConfigVerifiedStorageInterface::isOutOfSync().
   */
  public function isOutOfSync() {
    return $this->read() !== $this->readFromFile();
  }

  /**
   * Implements DrupalConfigVerifiedStorageInterface::write().
   */
  public function write($data) {
    $this->writeToActive($data);
    $this->copyToFile();
  }

  /**
   * Implements DrupalConfigVerifiedStorageInterface::delete().
   */
  public function delete() {
    $this->deleteFromActive();
    $this->deleteFile();
  }
}

/**
 * Represents an SQL-based configuration storage object.
 */
class DrupalVerifiedStorageSQL extends DrupalConfigVerifiedStorage {

  /**
   * Overrides DrupalConfigVerifiedStorage::read().
   */
  public function read() {
    // There are situations, like in the installer, where we may attempt a
    // read without actually having the database available. This is a
    // workaround and there is probably a better solution to be had at
    // some point.
    if (!empty($GLOBALS['databases']) && db_table_exists('config')) {
      return db_query('SELECT data FROM {config} WHERE name = :name', array(':name' => $this->name))->fetchField();
    }
  }

  /**
   * Implements DrupalConfigVerifiedStorageInterface::writeToActive().
   */
  public function writeToActive($data) {
    return db_merge('config')
      ->key(array('name' => $this->name))
      ->fields(array('data' => $data))
      ->execute();
  }

  /**
   * @todo
   */
  public function deleteFromActive() {
    db_delete('config')
      ->condition('name', $this->name)
      ->execute();
  }

  /**
   * Implements DrupalConfigVerifiedStorageInterface::getNamesWithPrefix().
   */
  static public function getNamesWithPrefix($prefix = '') {
    return db_query('SELECT name FROM {config} WHERE name LIKE :name', array(':name' => db_like($prefix) . '%'))->fetchCol();
  }
}

/**
 * Represents the default configuration storage object.
 */
class DrupalConfig {

  /**
   * The storage engine to save this config object to.
   *
   * @var DrupalConfigVerifiedStorageInterface
   */
  protected $_verifiedStorage;

  protected $data = array();

  /**
   * Constructs a DrupalConfig object.
   *
   * @param DrupalConfigVerifiedStorageInterface $verified_storage
   *   The storage engine where this config object should be saved.
   */
  public function __construct(DrupalConfigVerifiedStorageInterface $verified_storage) {
    $this->_verifiedStorage = $verified_storage;
    $this->read();
  }

  /**
   * Reads config data from the active store into our object.
   */
  public function read() {
    $active = (array) config_decode($this->_verifiedStorage->read());
    foreach ($active as $key => $value) {
      $this->set($key, $value);
    }
  }

  /**
   * Checks whether a particular value is overridden.
   *
   * @param $key
   *   @todo
   *
   * @return
   *   @todo
   */
  public function isOverridden($key) {
    return isset($this->_overrides[$key]);
  }

  /**
   * Gets data from this config object.
   *
   * @param $key
   *   A string that maps to a key within the configuration data. 
   *   For instance in the following XML:
   *  
   *   <foo>
   *     <bar>baz</bar>
   *   </foo>
   * 
   *  A key of 'foo.bar' would return the string 'baz'. However
   *  a key of 'foo' would return array('bar' => 'baz').
   *
   * If no key is specified, then the entire data array is returned.
   *
   * @return
   *   The data that was requested.
   */
  public function get($key = '') {
    if (empty($key)) {
      return $this->data;
    }
    else {
      $parts = explode('.', $key);
      if (count($parts) == 1) {
        return isset($this->data[$key]) ? $this->data[$key] : NULL;
      }
      else {
        return drupal_array_get_nested_value($this->data, $parts);
      }      
    }
  }

  /**
   * Sets value in this config object.
   *
   * @param $key
   *   @todo
   * @param $value
   *   @todo
   */
  public function set($key, $value) {
    $parts = explode('.', $key);
    if (count($parts) == 1) {
      $this->data[$key] = $value;
    }
    else {
      drupal_array_set_nested_value($this->data, $parts, $value);
    }
  }

  /**
   * Unsets value in this config object.
   *
   * @param $key
   *   @todo
   */
  public function clear($key) {
    $parts = explode('.', $key);
    if (count($parts) == 1) {
      unset($this->data[$key]);
    }
    else {
      drupal_array_unset_nested_value($this->data, $parts);
    }
  }

  /**
   * Saves the configuration object to disk as XML.
   */
  public function save() {
    $this->_verifiedStorage->write(config_encode($this->data));
  }

  /**
   * Deletes the configuration object on disk.
   */
  public function delete() {
    $this->_verifiedStorage->delete();
  }
}